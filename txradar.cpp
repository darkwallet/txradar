#include "txradar.hpp"

#include "util.hpp"

using std::placeholders::_1;
using std::placeholders::_2;
using std::placeholders::_3;

void log_nothing(
    bc::log_level level, const std::string& domain, const std::string& body)
{
}
void log_to_file(std::ofstream& file,
    bc::log_level level, const std::string& domain, const std::string& body)
{
    if (body.empty())
        return;
    file << level_repr(level);
    if (!domain.empty())
        file << " [" << domain << "]";
    file << ": " << body << std::endl;
}

txradar::txradar()
  : engine_(device_()), auth_(ctx_),
    hosts_(pool_), handshake_(pool_), network_(pool_),
    p2p_(pool_, hosts_, handshake_, network_)
{
    BITCOIN_ASSERT(ctx_.self());
    BITCOIN_ASSERT(auth_.self());
}
txradar::~txradar()
{
    pool_.stop();
    pool_.join();
}

void p2p_started(bc::network::protocol& p2p, const std::error_code& ec);

// Start the p2p network. Is called repeatedly until connected.
void start_p2p(bc::network::protocol& p2p)
{
    // Keep trying to connect until successful.
    p2p.start(std::bind(p2p_started, std::ref(p2p), _1));
}

// If there's an error then attempt to reconnect until successful.
void p2p_started(bc::network::protocol& p2p, const std::error_code& ec)
{
    if (ec)
    {
        bc::log_warning(LOG_TXR) << "Restarting connection...";
        start_p2p(p2p);
        return;
    }
    // Success. Call finish callback to signal success.
    bc::log_info(LOG_TXR) << "Radar started.";
}

void txradar::start(bool display_output, size_t threads, size_t number_hosts)
{
#ifdef ONLY_LOCALHOST_CONNECTIONS
    auth_.allow("127.0.0.1");
#endif

    pool_.spawn(threads);

    // Set connection counts.
    p2p_.set_max_outbound(number_hosts);
    // Notify us of new connections so we can subscribe to 'inv' packets.
    p2p_.subscribe_channel(
        std::bind(&txradar::connection_started, this, _1, _2));
    // Start connecting to p2p networks for broadcasting and monitor txs.
    start_p2p(p2p_);
}
void txradar::keep_pushing_count()
{
    // Keep pushing connections count
    czmqpp::socket socket(ctx_, ZMQ_PUB);
    BITCOIN_ASSERT(socket.self());
    int bind_rc = socket.bind(
        listen_transport(publish_connections_count_port));
    BITCOIN_ASSERT(bind_rc != -1);
    while (true)
    {
        czmqpp::message msg;
        const auto data = bc::to_little_endian(p2p_.total_connections());
        msg.append(bc::to_data_chunk(data));
        // Send it.
        bool rc = msg.send(socket);
        BITCOIN_ASSERT(rc);
        sleep(0.5);
    }
}

void txradar::connection_started(
    const std::error_code& ec, bc::network::channel_ptr node)
{
    if (ec)
    {
        bc::log_warning(LOG_TXR)
            << "Couldn't start connection: " << ec.message();
        return;
    }
    bc::log_info(LOG_TXR) << "Connection established.";
    node_id_type node_id =
        engine_() % std::numeric_limits<node_id_type>::max();
    // Subscribe to 'inv' packets.
    node->subscribe_inventory(
        std::bind(&txradar::inventory_received, this,
            _1, _2, node, node_id));
    // Resubscribe to new nodes.
    p2p_.subscribe_channel(
        std::bind(&txradar::connection_started, this, _1, _2));
}

template <typename Context, typename NodeID>
void notify_transaction(
    Context& ctx, NodeID node_id, const bc::hash_digest& tx_hash)
{
    static czmqpp::socket socket(ctx, ZMQ_PUB);
    static bool is_initialized = false;
    if (!is_initialized)
    {
        BITCOIN_ASSERT(socket.self());
        int bind_rc = socket.bind(listen_transport(notify_port));
        BITCOIN_ASSERT(bind_rc != -1);
        is_initialized = true;
        bc::log_info(LOG_TXR) << "Initialized ZMQ socket.";
    }
    bc::log_info(LOG_TXR) << "Sending (" << node_id << ", "
        << bc::encode_hash(tx_hash) << ")";
    // Create a message.
    czmqpp::message msg;
    // node_id
    const auto data_id = bc::to_little_endian(node_id);
    BITCOIN_ASSERT(data_id.size() == 4);
    msg.append(bc::to_data_chunk(data_id));
    // tx_hash
    const czmqpp::data_chunk data_hash(tx_hash.begin(), tx_hash.end());
    msg.append(data_hash);
    // Send it.
    bool rc = msg.send(socket);
    BITCOIN_ASSERT(rc);
}

void txradar::inventory_received(
    const std::error_code& ec, const bc::inventory_type& packet,
    bc::network::channel_ptr node, node_id_type node_id)
{
    if (ec)
    {
        bc::log_error(LOG_TXR) << "inventory: " << ec.message();
        return;
    }
    for (const bc::inventory_vector_type& ivec: packet.inventories)
    {
        if (ivec.type == bc::inventory_type_id::transaction)
        {
            notify_transaction(ctx_, node_id, ivec.hash);
        }
        else if (ivec.type == bc::inventory_type_id::block);
            // Do nothing.
        else
            bc::log_warning(LOG_TXR) << "Ignoring unknown inventory type";
    }
    // Resubscribe to 'inv' packets.
    node->subscribe_inventory(
        std::bind(&txradar::inventory_received, this,
            _1, _2, node, node_id));
}

