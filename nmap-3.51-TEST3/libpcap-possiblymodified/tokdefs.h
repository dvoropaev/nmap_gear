#ifndef YYERRCODE
#define YYERRCODE 256
#endif

#define DST 257
#define SRC 258
#define HOST 259
#define GATEWAY 260
#define NET 261
#define MASK 262
#define PORT 263
#define LESS 264
#define GREATER 265
#define PROTO 266
#define PROTOCHAIN 267
#define BYTE 268
#define ARP 269
#define RARP 270
#define IP 271
#define SCTP 272
#define TCP 273
#define UDP 274
#define ICMP 275
#define IGMP 276
#define IGRP 277
#define PIM 278
#define VRRP 279
#define ATALK 280
#define AARP 281
#define DECNET 282
#define LAT 283
#define SCA 284
#define MOPRC 285
#define MOPDL 286
#define TK_BROADCAST 287
#define TK_MULTICAST 288
#define NUM 289
#define INBOUND 290
#define OUTBOUND 291
#define LINK 292
#define GEQ 293
#define LEQ 294
#define NEQ 295
#define ID 296
#define EID 297
#define HID 298
#define HID6 299
#define AID 300
#define LSH 301
#define RSH 302
#define LEN 303
#define IPV6 304
#define ICMPV6 305
#define AH 306
#define ESP 307
#define VLAN 308
#define ISO 309
#define ESIS 310
#define ISIS 311
#define CLNP 312
#define STP 313
#define IPX 314
#define NETBEUI 315
#define OR 316
#define AND 317
#define UMINUS 318
typedef union {
	int i;
	bpf_u_int32 h;
	u_char *e;
	char *s;
	struct stmt *stmt;
	struct arth *a;
	struct {
		struct qual q;
		struct block *b;
	} blk;
	struct block *rblk;
} YYSTYPE;
extern YYSTYPE yylval;