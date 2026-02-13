#ifndef SOURCE_TYPE_H
#define SOURCE_TYPE_H

/**
 * @brief Network source type for adaptive buffer sizing
 *
 * Loopback: Same machine (localhost/127.x) - minimal buffers, ultra-low latency
 * LAN:      Local network (192.168.x, 10.x, 172.x) - larger buffers for network jitter
 * Remote:   Internet (Qobuz, Tidal, CDN) - larger buffers with reconnection support
 */
enum class SourceType {
    Loopback,
    LAN,
    Remote
};

#endif // SOURCE_TYPE_H
