#ifndef TXRADAR_TXRADAR_HPP
#define TXRADAR_TXRADAR_HPP

#include <czmq++/czmqpp.hpp>
#include <bitcoin/bitcoin.hpp>
#include "define.hpp"

class txradar
{
public:
    txradar();
    ~txradar();

    void start(bool display_output, size_t threads, size_t number_hosts);
    void keep_pushing_count();

private:
    typedef uint32_t node_id_type;

    bool connection_started(
        const std::error_code& ec, bc::network::channel_ptr node);
    bool inventory_received(
        const std::error_code& ec, const bc::inventory_type& packet,
        bc::network::channel_ptr node, node_id_type node_id);

    // Used for generating node IDs
    std::random_device device_;
    std::default_random_engine engine_;

    // ZeroMQ stuff
    czmqpp::context ctx_;
    czmqpp::authenticator auth_;

    // For logging
    std::ofstream outfile_, errfile_;

    // Threadpool
    bc::threadpool pool_;
    // Bitcoin network components.
    bc::network::hosts hosts_;
    bc::network::handshake handshake_;
    bc::network::peer network_;
    bc::network::protocol p2p_;
};

#endif

