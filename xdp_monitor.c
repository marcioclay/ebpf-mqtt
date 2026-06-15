#include <linux/bpf.h>
#include <linux/if_ether.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <bpf/bpf_endian.h>

#define TARGET_PORT 1883 

// Slow DoS
#define MIN_PKTS_CHECK 50
// Tamanho médio do pacote (Headers: Eth 14 + IP 20 + TCP 20 = 54 bytes).
// Se a média for inferior a 65 bytes por pacote, significa que não há envio de dados reais.
#define MIN_AVG_BYTES 65 

struct host_metrics {
    __u64 packet_count;
    __u64 byte_count;
    __u64 last_seen;
};

struct {
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(max_entries, 8192); 
    __type(key, __u32);        
    __type(value, struct host_metrics); 
} tcp_sessions SEC(".maps");

struct {
    __uint(type, BPF_MAP_TYPE_ARRAY);
    __uint(max_entries, 2);
    __type(key, __u32);
    __type(value, struct host_metrics); 
} proto_stats SEC(".maps");

SEC("xdp")
int xdp_monitor_prog(struct xdp_md *ctx) {
    void *data_end = (void *)(long)ctx->data_end;
    void *data = (void *)(long)ctx->data;

    // 1. Camada 2: Ethernet
    struct ethhdr *eth = data;
    if ((void *)(eth + 1) > data_end) return XDP_PASS;
    if (eth->h_proto != bpf_htons(ETH_P_IP)) return XDP_PASS;

    // 2. Camada 3: IP
    struct iphdr *iph = data + sizeof(struct ethhdr);
    if ((void *)(iph + 1) > data_end) return XDP_PASS;

    __u32 src_ip = iph->saddr;
    __u64 pkt_len = data_end - data; 
    __u64 now = bpf_ktime_get_ns();  
    __u32 stat_key;
    struct host_metrics *stats;

    // --- CENÁRIO 1: MITIGAÇÃO DDoS VOLUMÉTRICO (UDP) ---
    if (iph->protocol == 17) { 
        stat_key = 0;
        stats = bpf_map_lookup_elem(&proto_stats, &stat_key);
        if (stats) {
            __sync_fetch_and_add(&stats->packet_count, 1);
            __sync_fetch_and_add(&stats->byte_count, pkt_len);
            stats->last_seen = now;
        }
        // AÇÃO IPS: Bloqueia o pacote UDP imediatamente na placa de rede!
        return XDP_DROP; 
    }

    // --- CENÁRIO 2: MITIGAÇÃO SLOW DoS (TCP) ---
    if (iph->protocol == 6) { 
        int ip_hdr_len = iph->ihl * 4;
        if (ip_hdr_len < sizeof(struct iphdr)) return XDP_PASS;

        struct tcphdr *tcp = (void *)iph + ip_hdr_len;
        if ((void *)tcp + sizeof(struct tcphdr) > data_end) return XDP_PASS;

        if (tcp->dest == bpf_htons(TARGET_PORT)) {
            
            // A. Registo Global TCP
            stat_key = 1;
            stats = bpf_map_lookup_elem(&proto_stats, &stat_key);
            if (stats) {
                __sync_fetch_and_add(&stats->packet_count, 1);
                __sync_fetch_and_add(&stats->byte_count, pkt_len);
                stats->last_seen = now;
            }

            // B. Análise Comportamental por Host
            struct host_metrics *internal_stats = bpf_map_lookup_elem(&tcp_sessions, &src_ip);
            if (internal_stats) {
                __sync_fetch_and_add(&internal_stats->packet_count, 1);
                __sync_fetch_and_add(&internal_stats->byte_count, pkt_len);
                internal_stats->last_seen = now; 

                // AÇÃO IPS (Heurística): O atacante já enviou pacotes suficientes para análise?
                if (internal_stats->packet_count > MIN_PKTS_CHECK) {
                    // Calculamos a média (Multiplicação usada em vez de divisão para evitar erros no kernel BPF)
                    // Se o volume de dados for muito baixo em relação à quantidade de pacotes: é um ataque Slow!
                    if (internal_stats->byte_count < (internal_stats->packet_count * MIN_AVG_BYTES)) {
                        return XDP_DROP; // Bloqueia o atacante silenciosamente
                    }
                }

            } else {
                struct host_metrics new_host = {
                    .packet_count = 1,
                    .byte_count = pkt_len,
                    .last_seen = now
                };
                bpf_map_update_elem(&tcp_sessions, &src_ip, &new_host, BPF_ANY);
            }
        }
    }

    // Tráfego normal e legítimo passa para o sistema operativo
    return XDP_PASS;
}

char _license[] SEC("license") = "GPL";
