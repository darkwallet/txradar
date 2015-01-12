#ifndef TXRADAR_DEFINE_HPP
#define TXRADAR_DEFINE_HPP

#define ONLY_LOCALHOST_CONNECTIONS

const size_t notify_port = 7678;
const size_t publish_connections_count_port = 7679;

// How many connections radar should try to maintain.
const size_t target_connections = 40;

#define LOG_TXR "txradar"

#endif

