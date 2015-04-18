#include <common.h>
#include <limits.h>
#include <regex.h>

struct options_table_s {
  int id;             /* This is the value returned by find_option(). */
  char short_opt;     /* Single char short option (ou '\0' if none). */
  char *long_opt;     /* String for long option name (or NULL is none.) */
  int has_arg;        /* If option must have an argument, this is 1. */

  /* "private" part. */
  int  in_use;        /* Boolean used to check if option was already used. */
};

/* structure used in getConfigOptions() and get_ip_and_cidr_from_string() */
typedef struct {
  unsigned addr;
  unsigned cidr;
} T50_tmp_addr_t;

/* Local prototypes. */
static int check_if_option(char *);
static void check_options_rules(struct config_options * __restrict__);
static struct options_table_s *find_option(char *);
static void set_config_option(struct config_options * __restrict__, char *, int, char *);
static unsigned int toULong(char *, char *);
static unsigned int toULongCheckRange(char *, char *, unsigned int, unsigned int);
static void check_list_separators(char *, char *);
static void set_destination_addresses(char *, struct config_options * __restrict__); 
static void list_protocols(void);
static void set_default_protocol(struct config_options * __restrict__);
static int get_ip_and_cidr_from_string(char const * const, T50_tmp_addr_t *);

// Must disable this warning 'cause the initializations are right!
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"

/* Default command line interface options. */
/* NOTE: Using GCC structure initialization extension to
         make sure that all fields are initialized correctly. */
/* NOTE: As C standandard goes, any field not explicitly initialized
         will be filled with zero. */
static struct config_options co = {
  /* XXX COMMON OPTIONS                                                         */
  .threshold = 1000,                  /* default threshold                      */

  /* XXX IP HEADER OPTIONS  (IPPROTO_IP = 0)                                    */
  .ip = {
    .tos = IPTOS_PREC_IMMEDIATE,      /* default type of service                */
    .ttl = 255                        /* default time to live                   */
  },

  /* XXX ICMP HEADER OPTIONS (IPPROTO_ICMP = 1)                                 */
  .icmp = { .type = ICMP_ECHO },     /* default type                           */

  /* XXX IGMP HEADER OPTIONS (IPPROTO_IGMP = 2)                                 */
  .igmp = {
    .type = IGMP_HOST_MEMBERSHIP_QUERY, /* default type                           */
    .grec_type = 1,                     /* default group record type              */
    .sources = 2                        /* default number of sources              */
  },

  /* XXX TCP HEADER OPTIONS (IPPROTO_TCP = 6)                                   */
  .tcp = {
    .key_id = 1,                     /* default AO key ID         (RFC5925)    */
    .next_key = 1,                   /* default AO next key ID    (RFC5925)    */
    .nop = TCPOPT_EOL                /* default NOP option        (RFC793)     */
  },

  /* XXX EGP HEADER OPTIONS (IPPROTO_EGP = 8)                                   */
  .egp = {
    .type = EGP_NEIGHBOR_ACQUISITION,     /* default type                           */
    .code = EGP_ACQ_CODE_CEASE_CMD,       /* default code                           */
    .status = EGP_ACQ_STAT_ACTIVE_MODE    /* default status                         */
  },

  /* XXX RIP HEADER OPTIONS (IPPROTO_UDP = 17)                                  */
  .rip = {
    .command = 2,                    /* default command                        */
    .family = AF_INET,               /* default address family identifier      */
    .key_id = 1                      /* default authentication key ID          */
  },

  /* XXX DCCP HEADER OPTIONS (IPPROTO_DCCP = 33)                                */
  .dccp = { .type = DCCP_PKT_REQUEST }, /* default type                           */

  /* XXX RSVP HEADER OPTIONS (IPPROTO_RSVP = 46)                                */
  .rsvp = {
    .flags = 1,                      /* default flags                          */
    .type = RSVP_MESSAGE_TYPE_PATH,  /* default message type                   */
    .ttl = 254,                      /* default time to live                   */
    .session_proto = 1,              /* default SESSION protocol ID            */
    .session_flags = 1,              /* default SESSION flags                  */
    .time_refresh = 360,             /* default TIME refresh interval          */
    .error_flags = 2,                /* default ERROR flags                    */
    .error_code = 2,                 /* default ERROR code                     */
    .error_value = 8,                /* default ERROR value                    */
    .scope = 1,                      /* default number of SCOPE(s)             */
    .style_opt = 18,                 /* default STYLE option vector            */
    .tspec = 6                       /* default TSPEC service                  */
  },

  /* XXX EIGRP HEADER OPTIONS (IPPROTO_EIGRP = 88)                              */
  .eigrp = {
    .opcode = EIGRP_OPCODE_UPDATE,   /* default opcode                         */
    .type = EIGRP_TYPE_INTERNAL,     /* default type                           */
    .k1 = 1,                         /* default K1 value                       */
    .k3 = 1,                         /* default K3 value                       */
    .hold = 360,                     /* default hold time                      */
    .ios_major = 12,                 /* default IOS Major Version              */
    .ios_minor = 4,                  /* default IOS Minor Version              */
    .ver_major = 1,                  /* default EIGRP Major Version            */
    .ver_minor = 2,                  /* default EIGRP Minor Version            */
    .mtu = 1500,                     /* default maximum transmission unit      */
    .proto_id = 2,                   /* default external protocol ID           */
    .key_id = 1                      /* default authentication key ID          */
  },

  /* XXX OSPF HEADER OPTIONS (IPPROTO_OSPF = 89)                                */
  .ospf = {
    .type = OSPF_TYPE_HELLO,         /* default type                           */
    .hello_priority = 1,             /* default HELLO router priority          */
    .hello_dead = 360,               /* default HELLO router dead interval     */
    .dd_mtu = 1500,                  /* default DD MTU                         */
    .lsa_age = 360,                  /* default LSA age                        */
    .lsa_type = LSA_TYPE_ROUTER,     /* default LSA header type                */
    .lsa_link_type = LINK_TYPE_PTP,  /* default Router-LSA link type           */
    .key_id = 1                       /* default authentication key ID          */
  }

    /* NOTE: Add configuration structured values for new protocols here! */
};


/* Examplo de tabela de opções. */
static struct options_table_s options[] = {
  { OPTION_VERSION,               'v',  "version",          0 },
  { OPTION_HELP,                  'h',  "help",             0 },
  { OPTION_LIST_PROTOCOLS,        'l',  "list-protocols",   0 },
#ifdef __HAVE_TURBO__
  { OPTION_TURBO,                 0,    "turbo",            0 },
#endif
  { OPTION_THRESHOLD,             0,    "threshold",        1 },
  { OPTION_FLOOD,                 0,    "flood",            0 },
  { OPTION_ENCAPSULATED,          0,    "encapsulated",     0 },
  { OPTION_BOGUSCSUM,             'B',  "bogus-csum",       0 },

  /* XXX GRE HEADER OPTIONS (IPPROTO_GRE = 47) */
  { OPTION_GRE_SEQUENCE_PRESENT,  0,    "gre-seq-present",  0 },
  { OPTION_GRE_KEY_PRESENT,       0,    "gre-key-present",  0 },
  { OPTION_GRE_CHECKSUM_PRESENT,  0,    "gre-sum-present",  0 },
  { OPTION_GRE_KEY,               0,    "gre-key",          1 },
  { OPTION_GRE_SEQUENCE,          0,    "gre-sequence",     1 },
  { OPTION_GRE_SADDR,             0,    "gre-saddr",        1 },
  { OPTION_GRE_DADDR,             0,    "gre-daddr",        1 },

  /* XXX DCCP, TCP & UDP HEADER OPTIONS */
  { OPTION_SOURCE,                0,    "sport",            1 },
  { OPTION_DESTINATION,           0,    "dport",            1 },

  /* XXX IP HEADER OPTIONS (IPPROTO_IP = 0) */
  { OPTION_IP_SOURCE,             's',  "saddr",            1 },
  { OPTION_IP_TOS,                0,    "tos",              1 },
  { OPTION_IP_ID,                 0,    "id",               1 },
  { OPTION_IP_OFFSET,             0,    "frag-offset",      1 },
  { OPTION_IP_TTL,                0,    "ttl",              1 },
  { OPTION_IP_PROTOCOL,           'p',  "protocol",         1 },

  /* XXX ICMP HEADER OPTIONS (IPPROTO_ICMP = 1) */
  { OPTION_ICMP_TYPE,             0, "icmp-type",           1 },
  { OPTION_ICMP_CODE,             0, "icmp-code",           1 },
  { OPTION_ICMP_GATEWAY,          0, "icmp-gateway",        1 },
  { OPTION_ICMP_ID,               0, "icmp-id",             1 },
  { OPTION_ICMP_SEQUENCE,         0, "icmp-sequence",       1 },

  /* XXX IGMP HEADER OPTIONS (IPPROTO_IGMP = 2)                                      */
  { OPTION_IGMP_TYPE,             0, "igmp-type",           1 },
  { OPTION_IGMP_CODE,             0, "igmp-code",           1 },
  { OPTION_IGMP_GROUP,            0, "igmp-group",          1 },
  { OPTION_IGMP_QRV,              0, "igmp-qrv",            1 },
  { OPTION_IGMP_SUPPRESS,         0, "igmp-suppress",       0 },
  { OPTION_IGMP_QQIC,             0, "igmp-qqic",           1 },
  { OPTION_IGMP_GREC_TYPE,        0, "igmp-grec-type",      1 },
  { OPTION_IGMP_SOURCES,          0, "igmp-sources",        1 },
  { OPTION_IGMP_GREC_MULTICAST,   0, "igmp-multicast",      1 },
  { OPTION_IGMP_ADDRESS,          0, "igmp-address",        1 },

  /* XXX TCP HEADER OPTIONS (IPPROTO_TCP = 6)                                        */
  { OPTION_TCP_ACK,                0,    "acknowledge",     1 },
  { OPTION_TCP_SEQUENCE,           0,    "sequence",        1 },
  { OPTION_TCP_OFFSET,             0,    "data-offset",     1 },
  { OPTION_TCP_FIN,                'F',  "fin",             0 },
  { OPTION_TCP_SYN,                'S',  "syn",             0 },
  { OPTION_TCP_RST,                'R',  "rst",             0 },
  { OPTION_TCP_PSH,                'P',  "psh",             0 },
  { OPTION_TCP_ACK,                'A',  "ack",             0 },
  { OPTION_TCP_URG,                'U',  "urg",             0 },
  { OPTION_TCP_ECE,                'E',  "ece",             0 },
  { OPTION_TCP_CWR,                'C',  "cwr",             0 },
  { OPTION_TCP_WINDOW,             'W',  "window",          1 },
  { OPTION_TCP_URGENT_POINTER,     0,    "urg-pointer",     1 },
  { OPTION_TCP_MSS,                0,    "mss",             1 },
  { OPTION_TCP_WSOPT,              0,    "wscale",          1 },
  { OPTION_TCP_TSOPT,              0,    "tstamp",          1 },
  { OPTION_TCP_SACK_OK,            0,    "sack-ok",         0 },
  { OPTION_TCP_CC,                 0,    "cc",              1 },
  { OPTION_TCP_CC_NEW,             0,    "ccnew",           1 },
  { OPTION_TCP_CC_ECHO,            0,    "ccecho",          1 },
  { OPTION_TCP_SACK_EDGE,          0,    "sack",            1 },
  { OPTION_TCP_MD5_SIGNATURE,      0,    "md5-signature",   0 },
  { OPTION_TCP_AUTHENTICATION,     0,    "authentication",  0 },
  { OPTION_TCP_AUTH_KEY_ID,        0,    "auth-key-id",     1 },
  { OPTION_TCP_AUTH_NEXT_KEY,      0,    "auth-next-key",   1 },
  { OPTION_TCP_NOP,                0,    "nop",             0 },
  
  /* XXX EGP HEADER OPTIONS (IPPROTO_EGP = 8) */
  { OPTION_EGP_TYPE,               0, "egp-type",           1 },
  { OPTION_EGP_CODE,               0, "egp-code",           1 },
  { OPTION_EGP_STATUS,             0, "egp-status",         1 },
  { OPTION_EGP_AS,                 0, "egp-as",             1 },
  { OPTION_EGP_SEQUENCE,           0, "egp-sequence",       1 },
  { OPTION_EGP_HELLO,              0, "egp-hello",          1 },
  { OPTION_EGP_POLL,               0, "egp-poll",           1 },
  
  /* XXX RIP HEADER OPTIONS (IPPROTO_UDP = 17) */
  { OPTION_RIP_COMMAND,            0, "rip-command",        1 },
  { OPTION_RIP_FAMILY,             0, "rip-family",         1 },
  { OPTION_RIP_ADDRESS,            0, "rip-address",        1 },
  { OPTION_RIP_METRIC,             0, "rip-metric",         1 },
  { OPTION_RIP_DOMAIN,             0, "rip-domain",         1 },
  { OPTION_RIP_TAG,                0, "rip-tag",            1 },
  { OPTION_RIP_NETMASK,            0, "rip-netmask",        1 },
  { OPTION_RIP_NEXTHOP,            0, "rip-next-hop",       1 },
  { OPTION_RIP_AUTHENTICATION,     0, "rip-authentication", 0 },
  { OPTION_RIP_AUTH_KEY_ID,        0, "rip-auth-key-id",    1 },
  { OPTION_RIP_AUTH_SEQUENCE,      0, "rip-auth-sequence",  1 },
  
  /* XXX DCCP HEADER OPTIONS (IPPROTO_DCCP = 33) */
  { OPTION_DCCP_OFFSET,            0, "dccp-data-offset",   1 },
  { OPTION_DCCP_CSCOV,             0, "dccp-cscov",         1 },
  { OPTION_DCCP_CCVAL,             0, "dccp-ccval",         1 },
  { OPTION_DCCP_TYPE,              0, "dccp-type",          1 },
  { OPTION_DCCP_EXTEND,            0, "dccp-extended",      0 },
  { OPTION_DCCP_SEQUENCE_01,       0, "dccp-sequence-1",    1 },
  { OPTION_DCCP_SEQUENCE_02,       0, "dccp-sequence-2",    1 },
  { OPTION_DCCP_SEQUENCE_03,       0, "dccp-sequence-3",    1 },
  { OPTION_DCCP_SERVICE,           0, "dccp-service",       1 },
  { OPTION_DCCP_ACKNOWLEDGE_01,    0, "dccp-acknowledge-1", 1 },
  { OPTION_DCCP_ACKNOWLEDGE_02,    0, "dccp-acknowledge-2", 1 },
  { OPTION_DCCP_RESET_CODE,        0, "dccp-reset-code",    1 },
  
  /* XXX RSVP HEADER OPTIONS (IPPROTO_RSVP = 46) */
  { OPTION_RSVP_FLAGS,             0, "rsvp-flags",         1 },
  { OPTION_RSVP_TYPE,              0, "rsvp-type",          1 },
  { OPTION_RSVP_TTL,               0, "rsvp-ttl",           1 },
  { OPTION_RSVP_SESSION_ADDRESS,   0, "rsvp-session-addr",  1 },
  { OPTION_RSVP_SESSION_PROTOCOL,  0, "rsvp-session-proto", 1 },
  { OPTION_RSVP_SESSION_FLAGS,     0, "rsvp-session-flags", 1 },
  { OPTION_RSVP_SESSION_PORT,      0, "rsvp-session-port",  1 },
  { OPTION_RSVP_HOP_ADDRESS,       0, "rsvp-hop-addr",      1 },
  { OPTION_RSVP_HOP_IFACE,         0, "rsvp-hop-iface",     1 },
  { OPTION_RSVP_TIME_REFRESH,      0, "rsvp-time-refresh",  1 },
  { OPTION_RSVP_ERROR_ADDRESS,     0, "rsvp-error-addr",    1 },
  { OPTION_RSVP_ERROR_FLAGS,       0, "rsvp-error-flags",   1 },
  { OPTION_RSVP_ERROR_CODE,        0, "rsvp-error-code",    1 },
  { OPTION_RSVP_ERROR_VALUE,       0, "rsvp-error-value",   1 },
  { OPTION_RSVP_SCOPE,             0, "rsvp-scope",         1 },
  { OPTION_RSVP_SCOPE_ADDRESS,     0, "rsvp-address",       1 },
  { OPTION_RSVP_STYLE_OPTION,      0, "rsvp-style-option",  1 },
  { OPTION_RSVP_SENDER_ADDRESS,    0, "rsvp-sender-addr",   1 },
  { OPTION_RSVP_SENDER_PORT,       0, "rsvp-sender-port",   1 },
  { OPTION_RSVP_TSPEC_TRAFFIC,     0, "rsvp-tspec-traffic", 0 },
  { OPTION_RSVP_TSPEC_GUARANTEED,  0, "rsvp-tspec-guaranteed", 0 },
  { OPTION_RSVP_TSPEC_TOKEN_R,     0, "rsvp-tspec-r",       1 },
  { OPTION_RSVP_TSPEC_TOKEN_B,     0, "rsvp-tspec-b",       1 },
  { OPTION_RSVP_TSPEC_DATA_P,      0, "rsvp-tspec-p",       1 },
  { OPTION_RSVP_TSPEC_MINIMUM,     0, "rsvp-tspec-m",       1 },
  { OPTION_RSVP_TSPEC_MAXIMUM,     0, "rsvp-tspec-M",       1 },
  { OPTION_RSVP_ADSPEC_ISHOP,      0, "rsvp-adspec-ishop",  1 },
  { OPTION_RSVP_ADSPEC_PATH,       0, "rsvp-adspec-path",   1 },
  { OPTION_RSVP_ADSPEC_MINIMUM,    0, "rsvp-adspec-m",      1 },
  { OPTION_RSVP_ADSPEC_MTU,        0, "rsvp-adspec-mtu",    1 },
  { OPTION_RSVP_ADSPEC_GUARANTEED, 0, "rsvp-adspec-guaranteed", 0 },
  { OPTION_RSVP_ADSPEC_CTOT,       0, "rsvp-adspec-Ctot",   1 },
  { OPTION_RSVP_ADSPEC_DTOT,       0, "rsvp-adspec-Dtot",   1 },
  { OPTION_RSVP_ADSPEC_CSUM,       0, "rsvp-adspec-Csum",   1 },
  { OPTION_RSVP_ADSPEC_DSUM,       0, "rsvp-adspec-Dsum",   1 },
  { OPTION_RSVP_ADSPEC_CONTROLLED, 0, "rsvp-adspec-controlled", 0 },
  { OPTION_RSVP_CONFIRM_ADDR,      0, "rsvp-confirm-addr",  1 },
  
  /*O_ESP = 50) XXX IPSEC HEADER OPTIONS (IPPROTO_AH = 51 & IPPROT*/
  { OPTION_IPSEC_AH_LENGTH,        0, "ipsec-ah-length",    1 },
  { OPTION_IPSEC_AH_SPI,           0, "ipsec-ah-spi",       1 },
  { OPTION_IPSEC_AH_SEQUENCE,      0, "ipsec-ah-sequence",  1 },
  { OPTION_IPSEC_ESP_SPI,          0, "ipsec-esp-spi",      1 },
  { OPTION_IPSEC_ESP_SEQUENCE,     0, "ipsec-esp-sequence", 1 },
  
  /* XXX EIGRP HEADER OPTIONS (IPPROTO_EIGRP = 88) */
  { OPTION_EIGRP_OPCODE,           0, "eigrp-opcode",       1 },
  { OPTION_EIGRP_FLAGS,            0, "eigrp-flags",        1 },
  { OPTION_EIGRP_SEQUENCE,         0, "eigrp-sequence",     1 },
  { OPTION_EIGRP_ACKNOWLEDGE,      0, "eigrp-acknowledge",  1 },
  { OPTION_EIGRP_AS,               0, "eigrp-as",           1 },
  { OPTION_EIGRP_TYPE,             0, "eigrp-type",         1 },
  { OPTION_EIGRP_LENGTH,           0, "eigrp-length",       1 },
  { OPTION_EIGRP_K1,               0, "eigrp-k1",           1 },
  { OPTION_EIGRP_K2,               0, "eigrp-k2",           1 },
  { OPTION_EIGRP_K3,               0, "eigrp-k3",           1 },
  { OPTION_EIGRP_K4,               0, "eigrp-k4",           1 },
  { OPTION_EIGRP_K5,               0, "eigrp-k5",           1 },
  { OPTION_EIGRP_HOLD,             0, "eigrp-hold",         1 },
  { OPTION_EIGRP_IOS_VERSION,      0, "eigrp-ios-ver",      1 },
  { OPTION_EIGRP_PROTO_VERSION,    0, "eigrp-rel-ver",      1 },
  { OPTION_EIGRP_NEXTHOP,          0, "eigrp-next-hop",     1 },
  { OPTION_EIGRP_DELAY,            0, "eigrp-delay",        1 },
  { OPTION_EIGRP_BANDWIDTH,        0, "eigrp-bandwidth",    1 },
  { OPTION_EIGRP_MTU,              0, "eigrp-mtu",          1 },
  { OPTION_EIGRP_HOP_COUNT,        0, "eigrp-hop-count",    1 },
  { OPTION_EIGRP_LOAD,             0, "eigrp-load",         1 },
  { OPTION_EIGRP_RELIABILITY,      0, "eigrp-reliability",  1 },
  { OPTION_EIGRP_DESINATION,       0, "eigrp-daddr",        1 },
  { OPTION_EIGRP_SOURCE_ROUTER,    0, "eigrp-src-router",   1 },
  { OPTION_EIGRP_SOURCE_AS,        0, "eigrp-src-as",       1 },
  { OPTION_EIGRP_TAG,              0, "eigrp-tag",          1 },
  { OPTION_EIGRP_METRIC,           0, "eigrp-proto-metric", 1 },
  { OPTION_EIGRP_ID,               0, "eigrp-proto-id",     1 },
  { OPTION_EIGRP_EXTERNAL_FLAGS,   0, "eigrp-ext-flags",    1 },
  { OPTION_EIGRP_ADDRESS,          0, "eigrp-address",      1 },
  { OPTION_EIGRP_MULTICAST,        0, "eigrp-multicast",    1 },
  { OPTION_EIGRP_AUTHENTICATION,   0, "eigrp-authentication", 0 },
  { OPTION_EIGRP_AUTH_KEY_ID,      0, "eigrp-auth-key-id",  1 },
  
  /* XXX OSPF HEADER OPTIONS (IPPROTO_OSPF = 89) */
  { OPTION_OSPF_TYPE,              0,  "ospf-type",         1 },
  { OPTION_OSPF_LENGTH,            0,  "ospf-length",       1 },
  { OPTION_OSPF_ROUTER_ID,         0,  "ospf-router-id",    1 },
  { OPTION_OSPF_AREA_ID,           0,  "ospf-area-id",      1 },
  { OPTION_OSPF_MT,                0,  "ospf-option-MT",    0 },
  { OPTION_OSPF_E,                 '2',"ospf-option-E",     0 },
  { OPTION_OSPF_MC,                '3',"ospf-option-MC",    0 },
  { OPTION_OSPF_NP,                '4',"ospf-option-NP",    0 },
  { OPTION_OSPF_L,                 '5',"ospf-option-L",     0 },
  { OPTION_OSPF_DC,                '6',"ospf-option-DC",    0 },
  { OPTION_OSPF_O,                 '7',"ospf-option-O",     0 },
  { OPTION_OSPF_DN,                '8',"ospf-option-DN",    0 },
  { OPTION_OSPF_NETMASK,           0, "ospf-netmask",       1 },
  { OPTION_OSPF_HELLO_INTERVAL,    0, "ospf-hello-interval", 1 },
  { OPTION_OSPF_HELLO_PRIORITY,    0, "ospf-hello-priority", 1 },
  { OPTION_OSPF_HELLO_DEAD,        0, "ospf-hello-dead",    1 },
  { OPTION_OSPF_HELLO_DESIGN,      0, "ospf-hello-design",  1 },
  { OPTION_OSPF_HELLO_BACKUP,      0, "ospf-hello-backup",  1 },
  { OPTION_OSPF_HELLO_NEIGHBOR,    0, "ospf-neighbor",      1 },
  { OPTION_OSPF_HELLO_ADDRESS,     0, "ospf-address",       1 },
  { OPTION_OSPF_DD_MTU,            0, "ospf-dd-mtu",        1 },
  { OPTION_OSPF_DD_MASTER_SLAVE,   0, "ospf-dd-dbdesc-MS",  0 },
  { OPTION_OSPF_DD_MORE,           0, "ospf-dd-dbdesc-M",   0 },
  { OPTION_OSPF_DD_INIT,           0, "ospf-dd-dbdesc-I",   0 },
  { OPTION_OSPF_DD_OOBRESYNC,      0, "ospf-dd-dbdesc-R",   0 },
  { OPTION_OSPF_DD_SEQUENCE,       0, "ospf-dd-sequence",   1 },
  { OPTION_OSPF_DD_INCLUDE_LSA,    0, "ospf-dd-include-lsa", 0 },
  { OPTION_OSPF_LSA_AGE,           0, "ospf-lsa-age",       1 },
  { OPTION_OSPF_LSA_DO_NOT_AGE,    0, "ospf-lsa-do-not-age", 0 },
  { OPTION_OSPF_LSA_TYPE,          0, "ospf-lsa-type",      1 },
  { OPTION_OSPF_LSA_LSID,          0, "ospf-lsa-id",        1 },
  { OPTION_OSPF_LSA_ROUTER,        0, "ospf-lsa-router",    1 },
  { OPTION_OSPF_LSA_SEQUENCE,      0, "ospf-lsa-sequence",  1 },
  { OPTION_OSPF_LSA_METRIC,        0, "ospf-lsa-metric",    1 },
  { OPTION_OSPF_LSA_FLAG_BORDER,   0, "ospf-lsa-flag-B",    0 },
  { OPTION_OSPF_LSA_FLAG_EXTERNAL, 0, "ospf-lsa-flag-E",    0 },
  { OPTION_OSPF_LSA_FLAG_VIRTUAL,  0, "ospf-lsa-flag-V",    0 },
  { OPTION_OSPF_LSA_FLAG_WILD,     0, "ospf-lsa-flag-W",    0 },
  { OPTION_OSPF_LSA_FLAG_NSSA_TR,  0, "ospf-lsa-flag-NT",   0 },
  { OPTION_OSPF_LSA_LINK_ID,       0, "ospf-lsa-link-id",   1 },
  { OPTION_OSPF_LSA_LINK_DATA,     0, "ospf-lsa-link-data", 1 },
  { OPTION_OSPF_LSA_LINK_TYPE,     0, "ospf-lsa-link-type", 1 },
  { OPTION_OSPF_LSA_ATTACHED,      0, "ospf-lsa-attached",  1 },
  { OPTION_OSPF_LSA_LARGER,        0, "ospf-lsa-larger",    0 },
  { OPTION_OSPF_LSA_FORWARD,       0, "ospf-lsa-forward",   1 },
  { OPTION_OSPF_LSA_EXTERNAL,      0, "ospf-lsa-external",  1 },
  { OPTION_OSPF_VERTEX_ROUTER,     0, "ospf-vertex-router", 0 },
  { OPTION_OSPF_VERTEX_NETWORK,    0, "ospf-vertex-network", 0 },
  { OPTION_OSPF_VERTEX_ID,         0, "ospf-vertex-id",     1 },
  { OPTIONS_OSPF_LLS_OPTION_LR,    0, "ospf-lls-extended-LR", 0 },
  { OPTIONS_OSPF_LLS_OPTION_RS,    0, "ospf-lls-extended-RS", 0 },
  { OPTION_OSPF_AUTHENTICATION,    0, "ospf-authentication", 0 },
  { OPTION_OSPF_AUTH_KEY_ID,       0, "ospf-auth-key-id",   1 },
  { OPTION_OSPF_AUTH_SEQUENCE,     0, "ospf-auth-sequence", 1 },

  { 0, 0, NULL, 0 }
};

#pragma GCC diagnostic pop

/* Substitutes getConfigOptions() function.
   NOTE: This function expects &argv[0] as the first argument. */
struct config_options *parse_command_line(char **argv)
{
  struct options_table_s *ptbl;
  int num_options = 0;
  char *opt, *next_str, *dest_addr = NULL;

  /* Ugly hack! */
  set_default_protocol(&co);

  for (argv++; *argv; argv++)
  {
    opt = *argv;

    if (check_if_option(opt))
    {
      if ((ptbl = find_option(opt)) == NULL)
      {
        fprintf(stderr, "Unrecognized option '%s'.\n", opt);
        exit(EXIT_FAILURE);
      }

      /* Estou assumindo que cada opção só pode ser informada uma vez! */
      if (ptbl->in_use)
      {
        fprintf(stderr, "Option '%s' already given.\n", opt);
        exit(EXIT_FAILURE);
      }

      ptbl->in_use = 1;

      next_str = *(argv + 1);
      if (!ptbl->has_arg)
      {
        if (next_str != NULL && !check_if_option(next_str))
        {
          fprintf(stderr, "Option '%s' has no arguments.\n", opt);
          exit(EXIT_FAILURE);
        }
      }
      else
        if (next_str)
          argv++;

      set_config_option(&co, opt, ptbl->id, next_str);
    }
    else
    {
      /* Check if already got an address. */
      if (dest_addr)
      {
        ERROR("Target address given twice. Aborting...\n");
        exit(EXIT_FAILURE);
      }

      dest_addr = *argv;

      set_destination_addresses(dest_addr, &co); 
    }

    num_options++;
  }

  /* if '-h' (or '--help') option is given... */
  if ((ptbl = find_option("-h")) != NULL)
    if (ptbl->in_use)
    {
      if (num_options > 1)
        fprintf(stderr, "Option '-h' (or '--help') cannot be used with other options.\n");
      else
        usage();

      exit(EXIT_FAILURE);
    }

  /* if '-v' (or '--version') option is given... */
  if ((ptbl = find_option("-v")) != NULL)
    if (ptbl->in_use)
    {
      if (num_options > 1)
        fprintf(stderr, "Option '-v' (or '--version') cannot be used with other options.\n");
      else
        show_version();

      exit(EXIT_FAILURE);
    }

  /* if '-l' (or '--list-protocols') option is given... */
  if ((ptbl = find_option("-l")) != NULL)
    if (ptbl->in_use)
    {
      if (num_options > 1)
        fprintf(stderr, "Option '-l' (or '--list-protocols') cannot be used with other options.\n");
      else
        list_protocols();

      exit(EXIT_FAILURE);
    }

  check_options_rules(&co);

  return &co;
}

/* Check if the argument is an option. */
static int check_if_option(char *s) { return *s == '-'; }

/* NOTE: Ugly hack, but necessary! */ 
static void set_default_protocol(struct config_options * __restrict__ co)
{
  modules_table_t *ptbl;
  int i;

  co->ip.protocol = IPPROTO_TCP;
  for (i = 0, ptbl = mod_table; ptbl->protocol_id; i++, ptbl++)
  {
    if (ptbl->protocol_id == IPPROTO_TCP)
    {
      co->ip.protoname = i;
      break;
    }
  }
}

/* Scans the option table trying to find the option. 
   NOTE: "option" points to a string beginning with '-', always! */
static struct options_table_s *find_option(char *option)
{
  struct options_table_s *ptbl;
  
  for (ptbl = options; ptbl->id; ptbl++)
  {
    /* Is it a long option? */
    if (*(option + 1) == '-')
    {
      /* Find 'long option' */
      if ((ptbl->long_opt != NULL) && *(option + 2) != '\0')
        if (!strcmp(option + 2, ptbl->long_opt))
          return ptbl;
    }
    else
    {
      /* ... or, is it a short option? */
      if (*(option + 1) == ptbl->short_opt)
        return ptbl;
    }
  }

  /* Option not found. */
  return NULL;
}

/* Check rules for options, after we get them all. */
static void check_options_rules(struct config_options * __restrict__ co)
{
  /* Address field is mandatory! */
  if (!co->ip.daddr) 
  { 
    fprintf(stderr, "Target address needed.\n"); 
    exit(EXIT_FAILURE); 
  }

#ifdef __HAVE_TURBO__
  if (co->turbo && !co->flood)
  {
    fprintf(stderr, "Turbo mode only available when flooding.\n");
    exit(EXIT_FAILURE);
  }
#endif

  /* --flood and --threshold are mutually exclusive! */
  if (co->flood && find_option("threshold")->in_use)
  { 
    fprintf(stderr, "--flood and --threshold cannot be used at the same time.\n");
    exit(EXIT_FAILURE); 
  }

  /* Sanitizing the TCP Options SACK_Permitted and SACK Edges. */
  if (TEST_BITS(co->tcp.options, TCP_OPTION_SACK_OK) &&
      TEST_BITS(co->tcp.options, TCP_OPTION_SACK_EDGE))
  {
    ERROR("TCP options SACK-Permitted and SACK Edges are not allowed");
    exit(EXIT_FAILURE);
  }

  /* Sanitizing the TCP Options T/TCP CC and T/TCP CC.ECHO. */
  if (TEST_BITS(co->tcp.options, TCP_OPTION_CC) && (co->tcp.cc_echo))
  {
    ERROR("TCP options T/TCP CC and T/TCP CC.ECHO are not allowed");
    exit(EXIT_FAILURE);
  }

  if (!check_threshold(co))
    exit(EXIT_FAILURE);

  /* NOTE: Insert other rules here! */
}

/* Get the IP PROTOCOL. */
static void get_ip_protocol(struct config_options *co, char *arg)
{
  /* T50 protocol is a special case! Not in modules table! */
  if (!strcasecmp(arg, "T50"))
  {
    co->ip.protocol = IPPROTO_T50;
    co->ip.protoname = get_number_of_registered_modules();    /* Is this correct? */
  }
  else
  {
    /* Scan the modules table trying to get the protocol. */

    int i;

    for (i = 0; mod_table[i].acronym; i++)
      if (!strcasecmp(mod_table[i].acronym, arg))
      {
        co->ip.protocol = mod_table[co->ip.protoname = i].protocol_id;
        return;
      }

    fprintf(stderr, "Unknown protocol '%s'.\n", arg);
    exit(EXIT_FAILURE);
  }
}

static void set_destination_addresses(char *arg, struct config_options * __restrict__ co)
{
  char *p;
  T50_tmp_addr_t addr;

  if (get_ip_and_cidr_from_string(arg, &addr))
  {
    co->bits = addr.cidr;
    co->ip.daddr = htonl(addr.addr);
  }
  else
  {
    /* If get_ip_and_cidr_from_string() fails, it probably means that we have a name, instead of an IP. */

    /* Tries to resolve the name. */
    p = strtok(arg, "/");
    co->ip.daddr = resolv(p);

    /* Get cidr if any. */
    if ((p = strtok(NULL, "/")) != NULL)
      co->bits = atoi(p);
    else
      co->bits = 32;
  }
}

/* Setup an option. */
static void set_config_option(struct config_options * __restrict__ co, char *optname, int optid, char *arg)
{
  size_t counter;
  char *tmp_ptr;

  switch (optid)
  {
#ifdef __HAVE_TURBO__
    case OPTION_TURBO:                  co->turbo = TRUE; break;
#endif
    case OPTION_THRESHOLD:              co->threshold = toULong(optname, arg); break;
    case OPTION_FLOOD:                  co->flood = TRUE; break;
    case OPTION_ENCAPSULATED:           co->encapsulated = TRUE; break;
    case OPTION_BOGUSCSUM:              co->bogus_csum = TRUE; break;

    case OPTION_GRE_SEQUENCE_PRESENT:   co->gre.S = TRUE; break;
    case OPTION_GRE_KEY_PRESENT:        co->gre.K = TRUE; break;
    case OPTION_GRE_CHECKSUM_PRESENT:   co->gre.C = TRUE; break;
    case OPTION_GRE_KEY:                co->gre.key = toULong(optname, arg); break;
    case OPTION_GRE_SEQUENCE:           co->gre.sequence = toULong(optname, arg); break; 
    case OPTION_GRE_SADDR:              check_list_separators(optname, arg);
                                        co->gre.saddr = resolv(arg); break;
    case OPTION_GRE_DADDR:              check_list_separators(optname, arg);
                                        co->gre.daddr = resolv(arg); break;

    case OPTION_SOURCE:                 check_list_separators(optname, arg); 
                                        co->source = toULongCheckRange(optname, arg, 0, 65535); break;

    case OPTION_DESTINATION:            check_list_separators(optname, arg);
                                        co->dest   = toULongCheckRange(optname, arg, 0, 65535); break;

    case OPTION_IP_SOURCE:              check_list_separators(optname, arg);
                                        co->ip.saddr = resolv(arg); break;
    case OPTION_IP_TOS:                 co->ip.tos = toULong(optname, arg); break;
    case OPTION_IP_ID:                  co->ip.id = toULongCheckRange(optname, arg, 0, 65535); break;
    case OPTION_IP_OFFSET:              co->ip.frag_off = toULongCheckRange(optname, arg, 0, 0x1fff); break;
    case OPTION_IP_TTL:                 co->ip.ttl = toULongCheckRange(optname, arg, 0, 255); break;
    case OPTION_IP_PROTOCOL:            check_list_separators(optname, arg);
                                        get_ip_protocol(co, arg); break;

    case OPTION_ICMP_TYPE:              co->icmp.type = toULongCheckRange(optname, arg, 0, 255); break;
    case OPTION_ICMP_CODE:              co->icmp.code = toULongCheckRange(optname, arg, 0, 255); break;
    case OPTION_ICMP_GATEWAY:           check_list_separators(optname, arg);
                                        co->icmp.gateway = resolv(arg); break;
    case OPTION_ICMP_ID:                co->icmp.id = toULongCheckRange(optname, arg, 0, 255); break;
    case OPTION_ICMP_SEQUENCE:          co->icmp.sequence = toULongCheckRange(optname, arg, 0, 255); break;

    case OPTION_IGMP_TYPE:              co->igmp.type = toULongCheckRange(optname, arg, 0, 255); break;
    case OPTION_IGMP_CODE:              co->igmp.code = toULongCheckRange(optname, arg, 0, 255); break;
    case OPTION_IGMP_GROUP:             check_list_separators(optname, arg);
                                        co->igmp.group = resolv(arg); break;
    case OPTION_IGMP_QRV:               co->igmp.qrv = toULongCheckRange(optname, arg, 0, 7); break;
    case OPTION_IGMP_SUPPRESS:          co->igmp.suppress = TRUE; break;
    case OPTION_IGMP_QQIC:              co->igmp.qqic = toULongCheckRange(optname, arg, 0, 255); break;
    case OPTION_IGMP_GREC_TYPE:         co->igmp.grec_type = toULong(optname, arg); break;
    case OPTION_IGMP_SOURCES:           co->igmp.sources = toULongCheckRange(optname, arg, 0, 65535); break;
    case OPTION_IGMP_GREC_MULTICAST:    check_list_separators(optname, arg);
                                        co->igmp.grec_mca = resolv(arg); break;
    case OPTION_IGMP_ADDRESS:         
      /* '--igmp-address' can contain a list. */
      for (counter = 0, tmp_ptr = strtok(arg, ",");
           tmp_ptr && (counter < (sizeof(co->igmp.address) / sizeof(in_addr_t)));
           counter++, tmp_ptr = strtok(NULL, ","))
        co->igmp.address[counter] = resolv(tmp_ptr);
      co->igmp.sources = counter;
      break;

    case OPTION_TCP_FIN:                co->tcp.fin = TRUE; break;
    case OPTION_TCP_SYN:                co->tcp.syn = TRUE; break;
    case OPTION_TCP_RST:                co->tcp.rst = TRUE; break;
    case OPTION_TCP_PSH:                co->tcp.psh = TRUE; break;
    case OPTION_TCP_ACK:                co->tcp.ack = TRUE; break;
    case OPTION_TCP_URG:                co->tcp.urg = TRUE; break;
    case OPTION_TCP_ECE:                co->tcp.ece = TRUE; break;
    case OPTION_TCP_CWR:                co->tcp.cwr = TRUE; break;
    case OPTION_TCP_WINDOW:             co->tcp.window = toULongCheckRange(optname, arg, 0, 65535); break;
    case OPTION_TCP_MSS:                co->tcp.options |= TCP_OPTION_MSS;
                                        co->tcp.mss = toULong(optname, arg); break; 
    case OPTION_TCP_WSOPT:              co->tcp.options |= TCP_OPTION_WSOPT;           
                                        co->tcp.wsopt = toULong(optname, arg); break; 
    case OPTION_TCP_TSOPT:            
      /* This option can contain 2 values separated by ':'. */
      co->tcp.options |= TCP_OPTION_TSOPT;
      {
        unsigned long a = 0, b = 0;
        char *pch;

        if ((pch = strtok(arg, ".")) != NULL)
        {
          errno = 0;

          a = strtoul(pch, NULL, 10);
          if (!errno)
            goto error1;

          if ((pch = strtok(NULL, ".")) != NULL)
          {
            errno = 0;

            b = strtoul(pch, NULL, 10);
            if (!errno)
              goto error1;
          }

          if (a > ULONG_MAX || b > ULONG_MAX)
          {
            fprintf(stderr, "'%s' arguments are out of range.\n", optname);
            exit(EXIT_FAILURE);
          }
        }
        else
        {
error1:
          fprintf(stderr, "'%s' should receive an argument formated as 'n.[n]'.\n", optname);
          exit(EXIT_FAILURE);
        }

        co->tcp.tsval = a;
        co->tcp.tsecr = b;
      }
      break;
    case OPTION_TCP_SACK_OK:            co->tcp.options |= TCP_OPTION_SACK_OK; break;
    case OPTION_TCP_CC:                 co->tcp.options |= TCP_OPTION_CC; 
                                        co->tcp.cc = toULong(optname, arg); break;
    case OPTION_TCP_CC_NEW:             co->tcp.options |= TCP_OPTION_CC_NEXT; 
                                        co->tcp.cc_new = toULong(optname, arg); break;
    case OPTION_TCP_CC_ECHO:            co->tcp.options |= TCP_OPTION_CC_NEXT; 
                                        co->tcp.cc_echo = toULong(optname, arg); break;
    case OPTION_TCP_SACK_EDGE:        
      /* NOTE: This option expects 2 values, separated by ':'. */
      co->tcp.options |= TCP_OPTION_SACK_EDGE;
      #pragma message "Remember to fix this!"
      {
        unsigned int a = 0, b = 0;

        if (sscanf(arg, "%u:%u", &a, &b) > 0)
        {
          co->tcp.sack_left = a;
          co->tcp.sack_right = b;
        }
        else
        {
          fprintf(stderr, "'%s' MUST have 2 values separated by ':'.\n", optname);
          exit(EXIT_FAILURE);
        }
      }
      break;
    case OPTION_TCP_MD5_SIGNATURE:      co->tcp.md5 = !(co->tcp.auth = FALSE); break;
    case OPTION_TCP_AUTHENTICATION:     co->tcp.auth = !(co->tcp.md5 = FALSE); break;
    case OPTION_TCP_AUTH_KEY_ID:        co->tcp.key_id = toULong(optname, arg); break;
    case OPTION_TCP_AUTH_NEXT_KEY:      co->tcp.next_key = toULong(optname, arg); break;
    case OPTION_TCP_NOP:                co->tcp.nop = TCPOPT_NOP; break;
 
    case OPTION_EGP_TYPE:               co->egp.type = toULong(optname, arg);  break;    
    case OPTION_EGP_CODE:               co->egp.code = toULong(optname, arg);  break;
    case OPTION_EGP_STATUS:             co->egp.status = toULong(optname, arg); break;
    case OPTION_EGP_AS:                 co->egp.as = toULong(optname, arg); break;
    case OPTION_EGP_SEQUENCE:           co->egp.sequence = toULong(optname, arg); break;
    case OPTION_EGP_HELLO:              co->egp.hello = toULong(optname, arg); break;
    case OPTION_EGP_POLL:               co->egp.poll = toULong(optname, arg);  break;
 
    case OPTION_RIP_COMMAND:            co->rip.command = toULong(optname, arg); break;
    case OPTION_RIP_FAMILY:             co->rip.family = toULong(optname, arg); break;
    case OPTION_RIP_ADDRESS:            check_list_separators(optname, arg);
                                        co->rip.address = resolv(arg); break;
    case OPTION_RIP_METRIC:             co->rip.metric = toULong(optname, arg); break;
    case OPTION_RIP_DOMAIN:             co->rip.domain = toULong(optname, arg); break;
    case OPTION_RIP_TAG:                co->rip.tag = toULong(optname, arg); break;
    case OPTION_RIP_NETMASK:            co->rip.netmask = resolv(arg); break;  /* Is this correct? */
    case OPTION_RIP_NEXTHOP:            check_list_separators(optname, arg);
                                        co->rip.next_hop = resolv(arg); break;
    case OPTION_RIP_AUTHENTICATION:     co->rip.auth = TRUE; break;
    case OPTION_RIP_AUTH_KEY_ID:        co->rip.key_id = toULong(optname, arg); break;
    case OPTION_RIP_AUTH_SEQUENCE:      co->rip.sequence = toULong(optname, arg); break;
 
    case OPTION_DCCP_OFFSET:            co->dccp.doff = toULongCheckRange(optname, arg, 0, 65535); break;
    case OPTION_DCCP_CSCOV:             co->dccp.cscov = toULongCheckRange(optname, arg, 0, 15); break;
    case OPTION_DCCP_CCVAL:             co->dccp.ccval = toULongCheckRange(optname, arg, 0, 15); break;
    case OPTION_DCCP_TYPE:              co->dccp.type = toULongCheckRange(optname, arg, 0, 15); break;
    case OPTION_DCCP_EXTEND:            co->dccp.ext = TRUE; break;
    case OPTION_DCCP_SEQUENCE_01:       co->dccp.sequence_01 = toULong(optname, arg); break;
    case OPTION_DCCP_SEQUENCE_02:       co->dccp.sequence_02 = toULong(optname, arg); break;
    case OPTION_DCCP_SEQUENCE_03:       co->dccp.sequence_03 = toULong(optname, arg); break;
    case OPTION_DCCP_SERVICE:           co->dccp.service = toULong(optname, arg); break;
    case OPTION_DCCP_ACKNOWLEDGE_01:    co->dccp.acknowledge_01 = toULong(optname, arg); break;
    case OPTION_DCCP_ACKNOWLEDGE_02:    co->dccp.acknowledge_02 = toULong(optname, arg); break;
    case OPTION_DCCP_RESET_CODE:        co->dccp.rst_code = toULongCheckRange(optname, arg, 0, 255); break;
 
    case OPTION_RSVP_FLAGS:             co->rsvp.flags = toULongCheckRange(optname, arg, 0, 15); break;
    case OPTION_RSVP_TYPE:              co->rsvp.type = toULongCheckRange(optname, arg, 0, 255); break;
    case OPTION_RSVP_TTL:               co->rsvp.ttl = toULongCheckRange(optname, arg, 0, 255); break;
    case OPTION_RSVP_SESSION_ADDRESS:   check_list_separators(optname, arg);
                                        co->rsvp.session_addr = resolv(arg); break;
    case OPTION_RSVP_SESSION_PROTOCOL:  check_list_separators(optname, arg);
                                        co->rsvp.session_proto = toULongCheckRange(optname, arg, 0, 255); break;
    case OPTION_RSVP_SESSION_FLAGS:     co->rsvp.session_flags = toULongCheckRange(optname, arg, 0, 255); break;
    case OPTION_RSVP_SESSION_PORT:      check_list_separators(optname, arg);
                                        co->rsvp.session_port = toULongCheckRange(optname, arg, 0, 65535); break;
    case OPTION_RSVP_HOP_ADDRESS:       check_list_separators(optname, arg);
                                        co->rsvp.hop_addr = resolv(arg); break;
    case OPTION_RSVP_HOP_IFACE:         co->rsvp.hop_iface = toULong(optname, arg); break;
    case OPTION_RSVP_TIME_REFRESH:      co->rsvp.time_refresh = toULong(optname, arg); break;
    case OPTION_RSVP_ERROR_ADDRESS:     check_list_separators(optname, arg);
                                        co->rsvp.error_addr = resolv(arg); break;
    case OPTION_RSVP_ERROR_FLAGS:       co->rsvp.error_flags = toULongCheckRange(optname, arg, 0, 255); break;
    case OPTION_RSVP_ERROR_CODE:        co->rsvp.error_code = toULongCheckRange(optname, arg, 0, 255); break;
    case OPTION_RSVP_ERROR_VALUE:       co->rsvp.error_value = toULongCheckRange(optname, arg, 0, 65535); break;
    case OPTION_RSVP_SCOPE:             co->rsvp.scope = toULong(optname, arg); break;
    case OPTION_RSVP_SCOPE_ADDRESS:       
      /* '--rsvp-address' can have a list of addresses separated by ','. */
      for (counter = 0, tmp_ptr = strtok(arg, ",");
           tmp_ptr && (counter < (sizeof(co->rsvp.address) / sizeof(in_addr_t)));
           counter++, tmp_ptr = strtok(NULL, ","))
        co->rsvp.address[counter] = resolv(tmp_ptr);
      co->rsvp.scope = counter;
      break;
    case OPTION_RSVP_STYLE_OPTION:      co->rsvp.style_opt = toULong(optname, arg); break;
    case OPTION_RSVP_SENDER_ADDRESS:    check_list_separators(optname, arg);
                                        co->rsvp.sender_addr = resolv(arg); break;
    case OPTION_RSVP_SENDER_PORT:       check_list_separators(optname, arg);
                                        co->rsvp.sender_port = toULongCheckRange(optname, arg, 0, 65535); break;
    case OPTION_RSVP_TSPEC_TRAFFIC:     co->rsvp.tspec = TSPEC_TRAFFIC_SERVICE; break;
    case OPTION_RSVP_TSPEC_GUARANTEED:  co->rsvp.tspec = TSPEC_GUARANTEED_SERVICE; break;
    case OPTION_RSVP_TSPEC_TOKEN_R:     co->rsvp.tspec = TSPEC_TRAFFIC_SERVICE;
                                        co->rsvp.tspec_r = toULong(optname, arg); break;
    case OPTION_RSVP_TSPEC_TOKEN_B:     co->rsvp.tspec = TSPEC_TRAFFIC_SERVICE;
                                        co->rsvp.tspec_b = toULong(optname, arg); break;
    case OPTION_RSVP_TSPEC_DATA_P:      co->rsvp.tspec = TSPEC_TRAFFIC_SERVICE;
                                        co->rsvp.tspec_p = toULong(optname, arg); break;
    case OPTION_RSVP_TSPEC_MINIMUM:     co->rsvp.tspec = TSPEC_TRAFFIC_SERVICE;
                                        co->rsvp.tspec_m = toULong(optname, arg); break;
    case OPTION_RSVP_TSPEC_MAXIMUM:     co->rsvp.tspec = TSPEC_TRAFFIC_SERVICE;
                                        co->rsvp.tspec_M = toULong(optname, arg); break;
    case OPTION_RSVP_ADSPEC_ISHOP:      co->rsvp.adspec_hop = toULong(optname, arg); break;
    case OPTION_RSVP_ADSPEC_PATH:       co->rsvp.adspec_path = toULong(optname, arg); break;
    case OPTION_RSVP_ADSPEC_MINIMUM:    co->rsvp.adspec_minimum = toULong(optname, arg); break;
    case OPTION_RSVP_ADSPEC_MTU:        co->rsvp.adspec_mtu = toULong(optname, arg); break;
    case OPTION_RSVP_ADSPEC_GUARANTEED: co->rsvp.adspec = ADSPEC_GUARANTEED_SERVICE; break;
    case OPTION_RSVP_ADSPEC_CTOT:       co->rsvp.adspec = ADSPEC_GUARANTEED_SERVICE;
                                        co->rsvp.adspec_Ctot = toULong(optname, arg); break;
    case OPTION_RSVP_ADSPEC_DTOT:       co->rsvp.adspec = ADSPEC_GUARANTEED_SERVICE;
                                        co->rsvp.adspec_Dtot = toULong(optname, arg); break;
    case OPTION_RSVP_ADSPEC_CSUM:       co->rsvp.adspec = ADSPEC_GUARANTEED_SERVICE;
                                        co->rsvp.adspec_Csum = toULong(optname, arg); break;
    case OPTION_RSVP_ADSPEC_DSUM:       co->rsvp.adspec = ADSPEC_GUARANTEED_SERVICE;
                                        co->rsvp.adspec_Dsum = toULong(optname, arg); break;
    case OPTION_RSVP_ADSPEC_CONTROLLED: co->rsvp.adspec = ADSPEC_CONTROLLED_SERVICE; break;
    case OPTION_RSVP_CONFIRM_ADDR:      check_list_separators(optname, arg);
                                        co->rsvp.confirm_addr = resolv(arg); break;
 
    case OPTION_IPSEC_AH_LENGTH:        co->ipsec.ah_length = toULong(optname, arg); break;
    case OPTION_IPSEC_AH_SPI:           co->ipsec.ah_spi = toULong(optname, arg); break;
    case OPTION_IPSEC_AH_SEQUENCE:      co->ipsec.ah_sequence = toULong(optname, arg); break;
    case OPTION_IPSEC_ESP_SPI:          co->ipsec.esp_spi = toULong(optname, arg); break;
    case OPTION_IPSEC_ESP_SEQUENCE:     co->ipsec.esp_sequence = toULong(optname, arg); break;
 
    case OPTION_EIGRP_OPCODE:           co->eigrp.opcode = toULong(optname, arg); break;
    case OPTION_EIGRP_FLAGS:            co->eigrp.flags = toULong(optname, arg); break;
    case OPTION_EIGRP_SEQUENCE:         co->eigrp.sequence = toULong(optname, arg); break;
    case OPTION_EIGRP_ACKNOWLEDGE:      co->eigrp.acknowledge = toULong(optname, arg); break;
    case OPTION_EIGRP_AS:               co->eigrp.as = toULong(optname, arg); break;
    case OPTION_EIGRP_TYPE:             co->eigrp.type = toULong(optname, arg); break;
    case OPTION_EIGRP_LENGTH:           co->eigrp.length = toULong(optname, arg); break;
    case OPTION_EIGRP_K1:               co->eigrp.values |= EIGRP_KVALUE_K1;
                                        co->eigrp.k1 = toULong(optname, arg); break;
    case OPTION_EIGRP_K2:               co->eigrp.values |= EIGRP_KVALUE_K2;
                                        co->eigrp.k2 = toULong(optname, arg); break;
    case OPTION_EIGRP_K3:               co->eigrp.values |= EIGRP_KVALUE_K3;
                                        co->eigrp.k3 = toULong(optname, arg); break;
    case OPTION_EIGRP_K4:               co->eigrp.values |= EIGRP_KVALUE_K4;
                                        co->eigrp.k4 = toULong(optname, arg); break;
    case OPTION_EIGRP_K5:               co->eigrp.values |= EIGRP_KVALUE_K5;
                                        co->eigrp.k5 = toULong(optname, arg); break;
    case OPTION_EIGRP_HOLD:             co->eigrp.hold = toULong(optname, arg); break;
    case OPTION_EIGRP_IOS_VERSION:        
      #pragma message "Remember to fix this!"
      {
        unsigned int a = 0, b = 0;

        if (sscanf(arg, "%u.%u", &a, &b) > 0)
        {
          co->eigrp.ios_major = a;
          co->eigrp.ios_minor = b;
        }
        else
        {
          fprintf(stderr, "'%s' must have two values separated by '.'.\n", optname);
          exit(EXIT_FAILURE);
        }
      }
      break;
    case OPTION_EIGRP_PROTO_VERSION:      
      #pragma message "Remember to fix this!"
      {
        unsigned int a = 0, b = 0;

        if (sscanf(arg, "%u.%u", &a, &b) > 0)
        {
          co->eigrp.ver_major = a;
          co->eigrp.ver_minor = b;
        }
        else
        {
          fprintf(stderr, "'%s' must have two values separated by '.'.\n", optname);
          exit(EXIT_FAILURE);
        }
      }
      break;
    case OPTION_EIGRP_NEXTHOP:          check_list_separators(optname, arg);
                                        co->eigrp.next_hop = resolv(arg); break;
    case OPTION_EIGRP_DELAY:            co->eigrp.delay = toULong(optname, arg); break;
    case OPTION_EIGRP_BANDWIDTH:        co->eigrp.bandwidth = toULong(optname, arg); break;
    case OPTION_EIGRP_MTU:              co->eigrp.mtu = toULong(optname, arg); break;
    case OPTION_EIGRP_HOP_COUNT:        co->eigrp.hop_count = toULong(optname, arg); break;
    case OPTION_EIGRP_LOAD:             co->eigrp.load = toULong(optname, arg); break;
    case OPTION_EIGRP_RELIABILITY:      co->eigrp.reliability = toULong(optname, arg); break;
    case OPTION_EIGRP_DESINATION:         
      if ( (tmp_ptr = strchr(arg, '/')) != NULL )
      {
        *tmp_ptr++ = '\0';
        co->eigrp.prefix = toULong(optname, tmp_ptr);
      }
      co->eigrp.dest = resolv(arg);
      break;
    case OPTION_EIGRP_SOURCE_ROUTER:    co->eigrp.src_router = resolv(arg); break;
    case OPTION_EIGRP_SOURCE_AS:        co->eigrp.src_as = toULong(optname, arg); break;
    case OPTION_EIGRP_TAG:              co->eigrp.tag = toULong(optname, arg); break;
    case OPTION_EIGRP_METRIC:           co->eigrp.proto_metric = toULong(optname, arg); break;
    case OPTION_EIGRP_ID:               co->eigrp.proto_id = toULong(optname, arg); break;
    case OPTION_EIGRP_EXTERNAL_FLAGS:   co->eigrp.ext_flags = toULong(optname, arg); break;
    case OPTION_EIGRP_ADDRESS:          check_list_separators(optname, arg);
                                        co->eigrp.address = resolv(arg); break;
    case OPTION_EIGRP_MULTICAST:        co->eigrp.multicast = toULong(optname, arg); break;
    case OPTION_EIGRP_AUTHENTICATION:   co->eigrp.auth = TRUE; break;
    case OPTION_EIGRP_AUTH_KEY_ID:      co->eigrp.key_id = toULong(optname, arg); break;
 
    case OPTION_OSPF_TYPE:              co->ospf.type = toULong(optname, arg); break;
    case OPTION_OSPF_LENGTH:            co->ospf.length = toULong(optname, arg); break;
    case OPTION_OSPF_ROUTER_ID:         co->ospf.rid = resolv(arg); break;
    case OPTION_OSPF_AREA_ID:           co->ospf.AID = TRUE;
                                        co->ospf.aid = resolv(arg); break;
    case OPTION_OSPF_MT:                co->ospf.options |= OSPF_OPTION_TOS; break;
    case OPTION_OSPF_E:                 co->ospf.options |= OSPF_OPTION_EXTERNAL; break;
    case OPTION_OSPF_MC:                co->ospf.options |= OSPF_OPTION_MULTICAST; break;
    case OPTION_OSPF_NP:                co->ospf.options |= OSPF_OPTION_NSSA; break;
    case OPTION_OSPF_L:                 co->ospf.options |= OSPF_OPTION_LLS; break;
    case OPTION_OSPF_DC:                co->ospf.options |= OSPF_OPTION_DEMAND; break;
    case OPTION_OSPF_O:                 co->ospf.options |= OSPF_OPTION_OPAQUE; break;
    case OPTION_OSPF_DN:                co->ospf.options |= OSPF_OPTION_DOWN; break;
    case OPTION_OSPF_NETMASK:           co->ospf.netmask = resolv(arg); break;
    case OPTION_OSPF_HELLO_INTERVAL:    co->ospf.hello_interval = toULong(optname, arg); break;
    case OPTION_OSPF_HELLO_PRIORITY:    co->ospf.hello_priority = toULong(optname, arg); break;
    case OPTION_OSPF_HELLO_DEAD:        co->ospf.hello_dead = toULong(optname, arg); break;
    case OPTION_OSPF_HELLO_DESIGN:      co->ospf.hello_design = resolv(arg); break;
    case OPTION_OSPF_HELLO_BACKUP:      co->ospf.hello_backup = resolv(arg); break;
    case OPTION_OSPF_HELLO_NEIGHBOR:    co->ospf.neighbor = toULong(optname, arg); break;
    case OPTION_OSPF_HELLO_ADDRESS:       
      for (counter = 0, tmp_ptr = strtok(arg, ",");
           tmp_ptr && (counter < (sizeof(co->ospf.address) / sizeof(in_addr_t)));
           counter++, tmp_ptr = strtok(NULL, ","))
        co->ospf.address[counter] = resolv(tmp_ptr);
      co->ospf.neighbor = counter;
      break;
    case OPTION_OSPF_DD_MTU:            co->ospf.dd_mtu = toULong(optname, arg); break;
    case OPTION_OSPF_DD_MASTER_SLAVE:   co->ospf.dd_dbdesc |= DD_DBDESC_MSLAVE; break;
    case OPTION_OSPF_DD_MORE:           co->ospf.dd_dbdesc |= DD_DBDESC_MORE; break;
    case OPTION_OSPF_DD_INIT:           co->ospf.dd_dbdesc |= DD_DBDESC_INIT; break;
    case OPTION_OSPF_DD_OOBRESYNC:      co->ospf.dd_dbdesc |= DD_DBDESC_OOBRESYNC; break;
    case OPTION_OSPF_DD_SEQUENCE:       co->ospf.dd_sequence = toULong(optname, arg); break;
    case OPTION_OSPF_DD_INCLUDE_LSA:    co->ospf.dd_include_lsa = TRUE; break;
    case OPTION_OSPF_LSA_AGE:           co->ospf.lsa_age = toULong(optname, arg); break;
    case OPTION_OSPF_LSA_DO_NOT_AGE:    co->ospf.lsa_dage = TRUE; break;
    case OPTION_OSPF_LSA_TYPE:          co->ospf.lsa_type = toULong(optname, arg); break;
    case OPTION_OSPF_LSA_LSID:          co->ospf.lsa_lsid = resolv(arg); break;
    case OPTION_OSPF_LSA_ROUTER:        co->ospf.lsa_router = resolv(arg); break;
    case OPTION_OSPF_LSA_SEQUENCE:      co->ospf.lsa_sequence = toULong(optname, arg); break;
    case OPTION_OSPF_LSA_METRIC:        co->ospf.lsa_metric = toULong(optname, arg); break;
    case OPTION_OSPF_LSA_FLAG_BORDER:   co->ospf.lsa_flags |= ROUTER_FLAG_BORDER; break;
    case OPTION_OSPF_LSA_FLAG_EXTERNAL: co->ospf.lsa_flags |= ROUTER_FLAG_EXTERNAL; break;
    case OPTION_OSPF_LSA_FLAG_VIRTUAL:  co->ospf.lsa_flags |= ROUTER_FLAG_VIRTUAL; break;
    case OPTION_OSPF_LSA_FLAG_WILD:     co->ospf.lsa_flags |= ROUTER_FLAG_WILD; break;
    case OPTION_OSPF_LSA_FLAG_NSSA_TR:  co->ospf.lsa_flags |= ROUTER_FLAG_NSSA_TR; break;
    case OPTION_OSPF_LSA_LINK_ID:       co->ospf.lsa_link_id = resolv(arg); break;
    case OPTION_OSPF_LSA_LINK_DATA:     co->ospf.lsa_link_data = resolv(arg); break;
    case OPTION_OSPF_LSA_LINK_TYPE:     co->ospf.lsa_link_type = toULong(optname, arg); break;
    case OPTION_OSPF_LSA_ATTACHED:      co->ospf.lsa_attached = resolv(arg); break;
    case OPTION_OSPF_LSA_LARGER:        co->ospf.lsa_larger = TRUE; break;
    case OPTION_OSPF_LSA_FORWARD:       co->ospf.lsa_forward = resolv(arg); break;
    case OPTION_OSPF_LSA_EXTERNAL:      co->ospf.lsa_external = resolv(arg); break;
    case OPTION_OSPF_VERTEX_ROUTER:     co->ospf.vertex_type = VERTEX_TYPE_ROUTER; break;
    case OPTION_OSPF_VERTEX_NETWORK:    co->ospf.vertex_type = VERTEX_TYPE_NETWORK; break;
    case OPTION_OSPF_VERTEX_ID:         co->ospf.vertex_id = resolv(arg); break;
    case OPTIONS_OSPF_LLS_OPTION_LR:    co->ospf.lls_options = EXTENDED_OPTIONS_LR; break;
    case OPTIONS_OSPF_LLS_OPTION_RS:    co->ospf.lls_options = EXTENDED_OPTIONS_RS; break;
    case OPTION_OSPF_AUTHENTICATION:    co->ospf.auth = TRUE; break;
    case OPTION_OSPF_AUTH_KEY_ID:       co->ospf.key_id = toULong(optname, arg); break;
    case OPTION_OSPF_AUTH_SEQUENCE:     co->ospf.sequence = toULong(optname, arg); break;
  }
}

/* Tries to convert string to an unsigned value. */
static unsigned int toULong(char *optname, char *value)
{
  unsigned long n;

  assert(optname != NULL);
  assert(value != NULL);

  /* strtoul deals ok with hexadecimal, octal and decimal values. */
  errno = 0;    // errno is set only on error, so we have to reset it here.
  n = strtoul(value, NULL, 0);
  if (errno || n > UINT_MAX)
  {
    fprintf(stderr, "Invalid numeric value for option '%s'.\n", optname);
    exit(EXIT_FAILURE);
  }

  return (unsigned int)n;
}

/* Tries to convert string to unsigned int, checking range. 
   NOTE: 'min' MUST BE smaller than 'max'. */
static unsigned int toULongCheckRange(char *optname, char *value, unsigned int min, unsigned int max)
{
  unsigned int n;

  assert(min < max);

  n = toULong(optname, value);
  if (n < min || n > max)
  {
    fprintf(stderr, "Value out of range for option '%s'. Range must be between %u and %u.\n", optname, min, max);
    exit(EXIT_FAILURE);
  }

  return n;
}

/* Check if there are any separators on string. */
static void check_list_separators(char *optname, char *arg)
{
  assert(arg != NULL);

  if (strpbrk(arg, ",;:"))
  {
    fprintf(stderr, "Option '%s' do not accept a list.\n", optname);
    exit(EXIT_FAILURE);
  }
}

/* List procotolos on modules table */
static void list_protocols(void)
{
  modules_table_t *ptbl;
  int i;

  puts("List of supported protocols (--protocol):");

  for (i = 1, ptbl = mod_table; ptbl->func != NULL; ptbl++, i++)
    printf("\t%2d - %s\t(%s)\n",
           i,
           ptbl->acronym,
           ptbl->description);
}

/* POSIX Extended Regular Expression used to match IP addresses with optional CIDR. */
#define IP_REGEX "^([1-2]*[0-9]{1,2})" \
                 "(\\.[1-2]*[0-9]{1,2}){0,1}" \
                 "(\\.[1-2]*[0-9]{1,2}){0,1}" \
                 "(\\.[1-2]*[0-9]{1,2}){0,1}" \
                 "(/[0-9]{1,2}){0,1}$"

/* Auxiliary "match" macros. */
#define MATCH(a)        ((a).rm_so >= 0)
#define MATCH_LENGTH(a) ((a).rm_eo - (a).rm_so)

/* NOTE: There is a bug in strncpy() function.
         '\0' is not set at the end of substring. */
#define COPY_SUBSTRING(d, s, len) { \
  strncpy((d), (s), (len)); \
  *((char *)(d) + (len)) = '\0'; \
}

static int get_ip_and_cidr_from_string(char const * const addr, T50_tmp_addr_t *addr_ptr)
{
  regex_t re;
  regmatch_t rm[6];
  unsigned matches[5];
  int i, len;
  char *t;
  int bits;

  addr_ptr->addr = addr_ptr->cidr = 0;

  /* Try to compile the regular expression. */
  if (regcomp(&re, IP_REGEX, REG_EXTENDED))
    return FALSE;

  /* Try to execute regex against the addr string. */
  if (regexec(&re, addr, 6, rm, 0))
  {
    regfree(&re);
    return FALSE;
  }

  /* Allocate enough space for temporary string. */
  t = strdup(addr);
  if (t  == NULL)
  {
    perror("Cannot allocate temporary string");
    abort();
  }

  /* Convert IP octects matches. */
  len = MATCH_LENGTH(rm[1]);
  COPY_SUBSTRING(t, addr+rm[1].rm_so, len);
  matches[0] = atoi(t);

  bits = 32;  /* default is 32 bits netmask. */
  for (i = 2; i <= 4; i++)
  {
    if (MATCH(rm[i]))
    {
      len = MATCH_LENGTH(rm[i]) - 1;
      COPY_SUBSTRING(t, addr + rm[i].rm_so + 1, len);
      matches[i-1] = atoi(t);
    }
    else
    {
      /* if octect is missing, decrease 8 bits from netmask */
      bits -= 8;
      matches[i-1] = 0;
    }
  }

  /* Convert cidr match. */
  if (MATCH(rm[5]))
  {
    len = MATCH_LENGTH(rm[5]) - 1;
    COPY_SUBSTRING(t, addr + rm[5].rm_so + 1, len);

    if ((matches[4] = atoi(t)) == 0)
    {
      /* if cidr is actually '0', then it is an error! */
      free(t);
      regfree(&re);
      return FALSE;
    }
  }
  else
  {
    /* if cidr is not given, use the calculated one. */
    matches[4] = bits;
  }

  /* We don't need 't' string anymore. */
  free(t);

  /* Validate ip octects */
  for (i = 0; i < 4; i++)
    if (matches[i] > 255)
    {
      regfree(&re);
      return FALSE;
    }

  /* NOTE: Check 'bits' here! */
  /* Validate cidr. */
  if (matches[4] < CIDR_MINIMUM || matches[4] > CIDR_MAXIMUM)
  {
    char msg[64];

    sprintf(msg, "CIDR must be between %u and %u.\n", CIDR_MINIMUM, CIDR_MAXIMUM);
    ERROR(msg);

    regfree(&re);
    return FALSE;
  }

  regfree(&re);

  /* Prepare CIDR structure */
  addr_ptr->cidr = matches[4];
  addr_ptr->addr = ( matches[3]        |
                    (matches[2] << 8)  |
                    (matches[1] << 16) |
                    (matches[0] << 24)) &
                      (0xffffffffUL << (32 - addr_ptr->cidr));

  return TRUE;
}

