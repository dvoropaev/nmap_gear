
/***************************************************************************
 * osscan.cc -- Routines used for OS detection via TCP/IP fingerprinting.  *
 * For more information on how this works in Nmap, see my paper at         *
 * http://www.insecure.org/nmap/nmap-fingerprinting-article.html           *
 *                                                                         *
 ***********************IMPORTANT NMAP LICENSE TERMS************************
 *                                                                         *
 * The Nmap Security Scanner is (C) 1996-2004 Insecure.Com LLC. Nmap       *
 * is also a registered trademark of Insecure.Com LLC.  This program is    *
 * free software; you may redistribute and/or modify it under the          *
 * terms of the GNU General Public License as published by the Free        *
 * Software Foundation; Version 2.  This guarantees your right to use,     *
 * modify, and redistribute this software under certain conditions.  If    *
 * you wish to embed Nmap technology into proprietary software, we may be  *
 * willing to sell alternative licenses (contact sales@insecure.com).      *
 * Many security scanner vendors already license Nmap technology such as  *
 * our remote OS fingerprinting database and code, service/version         *
 * detection system, and port scanning code.                               *
 *                                                                         *
 * Note that the GPL places important restrictions on "derived works", yet *
 * it does not provide a detailed definition of that term.  To avoid       *
 * misunderstandings, we consider an application to constitute a           *
 * "derivative work" for the purpose of this license if it does any of the *
 * following:                                                              *
 * o Integrates source code from Nmap                                      *
 * o Reads or includes Nmap copyrighted data files, such as                *
 *   nmap-os-fingerprints or nmap-service-probes.                          *
 * o Executes Nmap                                                         *
 * o Integrates/includes/aggregates Nmap into an executable installer      *
 * o Links to a library or executes a program that does any of the above   *
 *                                                                         *
 * The term "Nmap" should be taken to also include any portions or derived *
 * works of Nmap.  This list is not exclusive, but is just meant to        *
 * clarify our interpretation of derived works with some common examples.  *
 * These restrictions only apply when you actually redistribute Nmap.  For *
 * example, nothing stops you from writing and selling a proprietary       *
 * front-end to Nmap.  Just distribute it by itself, and point people to   *
 * http://www.insecure.org/nmap/ to download Nmap.                         *
 *                                                                         *
 * We don't consider these to be added restrictions on top of the GPL, but *
 * just a clarification of how we interpret "derived works" as it applies  *
 * to our GPL-licensed Nmap product.  This is similar to the way Linus     *
 * Torvalds has announced his interpretation of how "derived works"        *
 * applies to Linux kernel modules.  Our interpretation refers only to     *
 * Nmap - we don't speak for any other GPL products.                       *
 *                                                                         *
 * If you have any questions about the GPL licensing restrictions on using *
 * Nmap in non-GPL works, we would be happy to help.  As mentioned above,  *
 * we also offer alternative license to integrate Nmap into proprietary    *
 * applications and appliances.  These contracts have been sold to many    *
 * security vendors, and generally include a perpetual license as well as  *
 * providing for priority support and updates as well as helping to fund   *
 * the continued development of Nmap technology.  Please email             *
 * sales@insecure.com for further information.                             *
 *                                                                         *
 * If you received these files with a written license agreement or         *
 * contract stating terms other than the (GPL) terms above, then that      *
 * alternative license agreement takes precedence over these comments.     *
 *                                                                         *
 * Source is provided to this software because we believe users have a     *
 * right to know exactly what a program is going to do before they run it. *
 * This also allows you to audit the software for security holes (none     *
 * have been found so far).                                                *
 *                                                                         *
 * Source code also allows you to port Nmap to new platforms, fix bugs,    *
 * and add new features.  You are highly encouraged to send your changes   *
 * to fyodor@insecure.org for possible incorporation into the main         *
 * distribution.  By sending these changes to Fyodor or one the            *
 * Insecure.Org development mailing lists, it is assumed that you are      *
 * offering Fyodor and Insecure.Com LLC the unlimited, non-exclusive right *
 * to reuse, modify, and relicense the code.  Nmap will always be          *
 * available Open Source, but this is important because the inability to   *
 * relicense code has caused devastating problems for other Free Software  *
 * projects (such as KDE and NASM).  We also occasionally relicense the    *
 * code to third parties as discussed above.  If you wish to specify       *
 * special license conditions of your contributions, just say so when you  *
 * send them.                                                              *
 *                                                                         *
 * This program is distributed in the hope that it will be useful, but     *
 * WITHOUT ANY WARRANTY; without even the implied warranty of              *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU       *
 * General Public License for more details at                              *
 * http://www.gnu.org/copyleft/gpl.html .                                  *
 *                                                                         *
 ***************************************************************************/

/* $Id: osscan.cc,v 1.20 2004/03/12 01:59:04 fyodor Exp $ */


#include "osscan.h"
#include "timing.h"
#include "NmapOps.h"

#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif

extern NmapOps o;
/*  predefined filters -- I need to kill these globals at some pont. */
extern unsigned long flt_dsthost, flt_srchost;
extern unsigned short flt_baseport;


FingerPrint *get_fingerprint(Target *target, struct seq_info *si) {
FingerPrint *FP = NULL, *FPtmp = NULL;
FingerPrint *FPtests[9];
struct AVal *seq_AVs;
u16 lastipid=0; /* For catching duplicate packets */
int last;
u32 timestamp = 0; /* TCP timestamp we receive back */
struct ip *ip;
struct tcphdr *tcp;
struct icmp *icmp;
struct timeval t1,t2;
int i;
pcap_t *pd = NULL;
int rawsd;
int tries = 0;
int newcatches;
int current_port = 0;
int testsleft;
int testno;
int  timeout;
int avnum;
unsigned int sequence_base;
unsigned int openport;
unsigned int bytes;
unsigned int closedport = 31337;
Port *tport = NULL;
char filter[512];
double seq_inc_sum = 0;
unsigned int  seq_avg_inc = 0;
struct udpprobeinfo *upi = NULL;
u32 seq_gcd = 1;
u32 seq_diffs[NUM_SEQ_SAMPLES];
u32 ts_diffs[NUM_SEQ_SAMPLES];
unsigned long time_usec_diffs[NUM_SEQ_SAMPLES];
struct timeval seq_send_times[NUM_SEQ_SAMPLES];
int ossofttimeout, oshardtimeout;
int seq_packets_sent = 0;
int seq_response_num; /* response # for sequencing */
double avg_ts_hz = 0.0; /* Avg. amount that timestamps incr. each second */
struct link_header linkhdr;

if (target->timedout)
  return NULL;

/* The seqs must start out as zero for the si struct */
memset(si->seqs, 0, sizeof(si->seqs));
si->ipid_seqclass = IPID_SEQ_UNKNOWN;
si->ts_seqclass = TS_SEQ_UNKNOWN;
si->lastboot = 0;

/* Init our fingerprint tests to each be NULL */
memset(FPtests, 0, sizeof(FPtests)); 
get_random_bytes(&sequence_base, sizeof(unsigned int));
/* Init our raw socket */
 if ((rawsd = socket(AF_INET, SOCK_RAW, IPPROTO_RAW)) < 0 )
   pfatal("socket trobles in get_fingerprint");
 unblock_socket(rawsd);
 broadcast_socket(rawsd);

 /* Now for the pcap opening nonsense ... */
 /* Note that the snaplen is 152 = 64 byte max IPhdr + 24 byte max link_layer
  * header + 64 byte max TCP header.  Had to up it for UDP test
  */

ossofttimeout = MAX(200000, target->to.timeout);
oshardtimeout = MAX(500000, 5 * target->to.timeout);

 pd = my_pcap_open_live(target->device, /*650*/ 8192,  (o.spoofsource)? 1 : 0, (ossofttimeout + 500)/ 1000);

if (o.debugging)
   log_write(LOG_STDOUT, "Wait time is %dms\n", (ossofttimeout +500)/1000);

 flt_srchost = target->v4host().s_addr;
 flt_dsthost = target->v4source().s_addr;

snprintf(filter, sizeof(filter), "dst host %s and (icmp or (tcp and src host %s))", inet_ntoa(target->v4source()), target->targetipstr());
 
 set_pcap_filter(target, pd, flt_icmptcp, filter);
 target->osscan_performed = 1; /* Let Nmap know that we did try an OS scan */

 /* Lets find an open port to use */
 openport = (unsigned long) -1;
 target->FPR->osscan_opentcpport = -1;
 target->FPR->osscan_closedtcpport = -1;
 tport = NULL;
 if ((tport = target->ports.nextPort(NULL, IPPROTO_TCP, PORT_OPEN, false))) {
   openport = tport->portno;
   target->FPR->osscan_opentcpport = tport->portno;
 }
 
 /* Now we should find a closed port */
 if ((tport = target->ports.nextPort(NULL, IPPROTO_TCP, PORT_CLOSED, false))) {
   closedport = tport->portno;
   target->FPR->osscan_closedtcpport = tport->portno;
 } else if ((tport = target->ports.nextPort(NULL, IPPROTO_TCP, PORT_UNFIREWALLED, false))) {
   /* Well, we will settle for unfiltered */
   closedport = tport->portno;
 } else {
   closedport = (get_random_uint() % 14781) + 30000;
 }

if (o.verbose && openport != (unsigned long) -1)
  log_write(LOG_STDOUT, "For OSScan assuming that port %d is open and port %d is closed and neither are firewalled\n", openport, closedport);

 current_port = o.magic_port + NUM_SEQ_SAMPLES +1;
 
 /* Now lets do the NULL packet technique */
 testsleft = (openport == (unsigned long) -1)? 4 : 8;
 FPtmp = NULL;
 tries = 0;
 do { 
   newcatches = 0;
   if (openport != (unsigned long) -1) {   
     /* Test 1 */
     if (!FPtests[1]) {     
       if (o.scan_delay) enforce_scan_delay(NULL);
       send_tcp_raw_decoys(rawsd, target->v4hostip(), o.ttl, current_port, 
			   openport, sequence_base, 0,TH_ECE|TH_SYN, 0, (u8 *) "\003\003\012\001\002\004\001\011\010\012\077\077\077\077\000\000\000\000\000\000" , 20, NULL, 0);
     }
     
     /* Test 2 */
     if (!FPtests[2]) {     
       if (o.scan_delay) enforce_scan_delay(NULL);
       send_tcp_raw_decoys(rawsd, target->v4hostip(), o.ttl, current_port +1, 
			   openport, sequence_base, 0,0, 0, (u8 *) "\003\003\012\001\002\004\001\011\010\012\077\077\077\077\000\000\000\000\000\000" , 20, NULL, 0);
     }

     /* Test 3 */
     if (!FPtests[3]) {     
       if (o.scan_delay) enforce_scan_delay(NULL);
       send_tcp_raw_decoys(rawsd, target->v4hostip(), o.ttl, current_port +2, 
			   openport, sequence_base, 0,TH_SYN|TH_FIN|TH_URG|TH_PUSH, 0,(u8 *) "\003\003\012\001\002\004\001\011\010\012\077\077\077\077\000\000\000\000\000\000" , 20, NULL, 0);
     }

     /* Test 4 */
     if (!FPtests[4]) {     
       if (o.scan_delay) enforce_scan_delay(NULL);
       send_tcp_raw_decoys(rawsd, target->v4hostip(), o.ttl, current_port +3, 
			   openport, sequence_base, 0,TH_ACK, 0, (u8 *) "\003\003\012\001\002\004\001\011\010\012\077\077\077\077\000\000\000\000\000\000" , 20, NULL, 0);
     }
   }
   
   /* Test 5 */
   if (!FPtests[5]) {   
     if (o.scan_delay) enforce_scan_delay(NULL);
     send_tcp_raw_decoys(rawsd, target->v4hostip(), o.ttl, current_port +4,
			 closedport, sequence_base, 0,TH_SYN, 0, (u8 *) "\003\003\012\001\002\004\001\011\010\012\077\077\077\077\000\000\000\000\000\000" , 20, NULL, 0);
   }

     /* Test 6 */
   if (!FPtests[6]) {   
     if (o.scan_delay) enforce_scan_delay(NULL);
     send_tcp_raw_decoys(rawsd, target->v4hostip(), o.ttl, current_port +5, 
			 closedport, sequence_base, 0,TH_ACK, 0, (u8 *) "\003\003\012\001\002\004\001\011\010\012\077\077\077\077\000\000\000\000\000\000" , 20, NULL, 0);
   }

     /* Test 7 */
   if (!FPtests[7]) {
     if (o.scan_delay) enforce_scan_delay(NULL);   
     send_tcp_raw_decoys(rawsd, target->v4hostip(), o.ttl, current_port +6, 
			 closedport, sequence_base, 0,TH_FIN|TH_PUSH|TH_URG, 0, (u8 *) "\003\003\012\001\002\004\001\011\010\012\077\077\077\077\000\000\000\000\000\000" , 20, NULL, 0);
   }

   /* Test 8 */
   if (!FPtests[8]) {
     if (o.scan_delay) enforce_scan_delay(NULL);
     upi = send_closedudp_probe(rawsd, target->v4hostip(), o.magic_port, closedport);
   }
   gettimeofday(&t1, NULL);
   timeout = 0;

   /* Insure we haven't overrun our allotted time ... */
   if (o.host_timeout && (TIMEVAL_MSEC_SUBTRACT(t1, target->host_timeout) >= 0))
     {
       target->timedout = 1;
       goto osscan_timedout;
     }
   while(( ip = (struct ip*) readip_pcap(pd, &bytes, oshardtimeout, NULL, &linkhdr)) && !timeout) {
     gettimeofday(&t2, NULL);
     if (TIMEVAL_SUBTRACT(t2,t1) > oshardtimeout) {
       timeout = 1;
     }
     if (o.host_timeout && (TIMEVAL_MSEC_SUBTRACT(t2, target->host_timeout) >= 0))
       {
	 target->timedout = 1;
	 goto osscan_timedout;
       }

     if (bytes < (4 * ip->ip_hl) + 4U || bytes < 20)
       continue;
     setTargetMACIfAvailable(target, &linkhdr, ip, 0);
     if (ip->ip_p == IPPROTO_TCP) {
       tcp = ((struct tcphdr *) (((char *) ip) + 4 * ip->ip_hl));
       testno = ntohs(tcp->th_dport) - current_port + 1;
       if (testno <= 0 || testno > 7)
	 continue;
       if (o.debugging > 1)
	 log_write(LOG_STDOUT, "Got packet for test number %d\n", testno);
       if (FPtests[testno]) continue;
       testsleft--;
       newcatches++;
       FPtests[testno] = (FingerPrint *) safe_zalloc(sizeof(FingerPrint));
       FPtests[testno]->results = fingerprint_iptcppacket(ip, 265, sequence_base);
       FPtests[testno]->name = (testno == 1)? "T1" : (testno == 2)? "T2" : (testno == 3)? "T3" : (testno == 4)? "T4" : (testno == 5)? "T5" : (testno == 6)? "T6" : (testno == 7)? "T7" : "PU";
     } else if (ip->ip_p == IPPROTO_ICMP) {
       icmp = ((struct icmp *)  (((char *) ip) + 4 * ip->ip_hl));
       /* It must be a destination port unreachable */
       if (icmp->icmp_type != 3 || icmp->icmp_code != 3) {
	 /* This ain't no stinking port unreachable! */
	 continue;
       }
       if (bytes < (unsigned int) ntohs(ip->ip_len)) {
	 error("We only got %d bytes out of %d on our ICMP port unreachable packet, skipping", bytes, ntohs(ip->ip_len));
	 continue;
       }
       if (FPtests[8]) continue;
       FPtests[8] = (FingerPrint *) safe_zalloc(sizeof(FingerPrint));
       FPtests[8]->results = fingerprint_portunreach(ip, upi);
       if (FPtests[8]->results) {       
	 FPtests[8]->name = "PU";
	 testsleft--;
	 newcatches++;
       } else {
	 free(FPtests[8]);
	 FPtests[8] = NULL;
       }
     }
    if (testsleft == 0)
      break;
   }     
 } while ( testsleft > 0 && (tries++ < 5 && (newcatches || tries == 1)));

 si->responses = 0;
 timeout = 0; 
 gettimeofday(&t1,NULL);
 /* Next we send our initial NUM_SEQ_SAMPLES SYN packets  */
 if (openport != (unsigned long) -1) {
   seq_packets_sent = 0;
   while (seq_packets_sent < NUM_SEQ_SAMPLES) {
     if (o.scan_delay) enforce_scan_delay(NULL);
     send_tcp_raw_decoys(rawsd, target->v4hostip(), o.ttl, 
			 o.magic_port + seq_packets_sent + 1, 
			 openport, 
			 sequence_base + seq_packets_sent + 1, 0, 
			 TH_SYN, 0 , (u8 *) "\003\003\012\001\002\004\001\011\010\012\077\077\077\077\000\000\000\000\000\000" , 20, NULL, 0);
     if (!o.scan_delay)
       usleep( MAX(110000, target->to.srtt)); /* Main reason we wait so long is that we need to spend more than .5 seconds to detect 2HZ timestamp sequencing -- this also should make ISN sequencing more regular */
   
     gettimeofday(&seq_send_times[seq_packets_sent], NULL);
     seq_packets_sent++;
     
     /* Now we collect  the replies */
     
     while(si->responses < seq_packets_sent && !timeout) {
       
       if (seq_packets_sent == NUM_SEQ_SAMPLES)
	 ip = (struct ip*) readip_pcap(pd, &bytes, oshardtimeout, NULL, &linkhdr);
       else ip = (struct ip*) readip_pcap(pd, &bytes, 10, NULL, &linkhdr);
       
       gettimeofday(&t2, NULL);
       /*     error("DEBUG: got a response (len=%d):\n", bytes);  */
       /*     lamont_hdump((unsigned char *) ip, bytes); */
       /* Insure we haven't overrun our allotted time ... */
       if (o.host_timeout && (TIMEVAL_MSEC_SUBTRACT(t2, target->host_timeout) >= 0))
	 {
	   target->timedout = 1;
	   goto osscan_timedout;
	 }
       if (!ip) { 
	 if (seq_packets_sent < NUM_SEQ_SAMPLES)
	   break;
	 if (TIMEVAL_SUBTRACT(t2,t1) > ossofttimeout)
	   timeout = 1;
	 continue; 
       } else if (TIMEVAL_SUBTRACT(t2,t1) > oshardtimeout) {
	 timeout = 1;
       }		  
       if (lastipid != 0 && ip->ip_id == lastipid) {
	 /* Probably a duplicate -- this happens sometimes when scanning localhost */
	 continue;
       }
       lastipid = ip->ip_id;

       if (bytes < (4 * ip->ip_hl) + 4U || bytes < 20)
	 continue;
       setTargetMACIfAvailable(target, &linkhdr, ip, 0);
       if (ip->ip_p == IPPROTO_TCP) {
	 /*       readtcppacket((char *) ip, ntohs(ip->ip_len));  */
	 tcp = ((struct tcphdr *) (((char *) ip) + 4 * ip->ip_hl));
	 if (ntohs(tcp->th_dport) < o.magic_port || ntohs(tcp->th_dport) - o.magic_port > NUM_SEQ_SAMPLES || ntohs(tcp->th_sport) != openport) {
	   continue;
	 }
	 if ((tcp->th_flags & TH_RST)) {
	   /*	 readtcppacket((char *) ip, ntohs(ip->ip_len));*/	 
	   if (si->responses == 0) {	 
	     fprintf(stderr, "WARNING:  RST from port %hu -- is this port really open?\n", openport);
	     /* We used to quit in this case, but left-overs from a SYN
		scan or lame-ass TCP wrappers can cause this! */
	   } 
	   continue;
	 } else if ((tcp->th_flags & (TH_SYN|TH_ACK)) == (TH_SYN|TH_ACK)) {
	   /*	error("DEBUG: response is SYN|ACK to port %hu\n", ntohs(tcp->th_dport)); */
	   /*readtcppacket((char *)ip, ntohs(ip->ip_len));*/
	   /* We use the ACK value to match up our sent with rcv'd packets */
	   seq_response_num = (ntohl(tcp->th_ack) - 2 - 
			       sequence_base); 
	   if (seq_response_num < 0 || seq_response_num >= seq_packets_sent) {
	     /* BzzT! Value out of range */
	     if (o.debugging) {
	       error("Unable to associate os scan response with sent packet (received ack: %lX; sequence base: %lX. Packet:", (unsigned long) ntohl(tcp->th_ack), (unsigned long) sequence_base);
	       readtcppacket((unsigned char *)ip,BSDUFIX(ip->ip_len));
	     }
	     seq_response_num = si->responses;
	   }
	   si->responses++;
	   si->seqs[seq_response_num] = ntohl(tcp->th_seq); /* TCP ISN */
	   si->ipids[seq_response_num] = ntohs(ip->ip_id);
	   if ((gettcpopt_ts(tcp, &timestamp, NULL) == 0))
	     si->ts_seqclass = TS_SEQ_UNSUPPORTED;
	   else {
	     if (timestamp == 0) {
	       si->ts_seqclass = TS_SEQ_ZERO;
	     }
	   }
	   si->timestamps[seq_response_num] = timestamp;
	   /*           printf("Response #%d -- ipid=%hu ts=%i\n", seq_response_num, ntohs(ip->ip_id), timestamp); */
	   if (si->responses > 1) {
	     seq_diffs[si->responses-2] = MOD_DIFF(ntohl(tcp->th_seq), si->seqs[si->responses-2]);
	   }      
	 }
       }
     }
   }

   /* Now we make sure there are no gaps in our response array ... */
   for(i=0, si->responses=0; i < seq_packets_sent; i++) {
     if (si->seqs[i] != 0) /* We found a good one */ {
       if (si->responses < i) {
	 si->seqs[si->responses] = si->seqs[i];
	 si->ipids[si->responses] = si->ipids[i];
	 si->timestamps[si->responses] = si->timestamps[i];
	 seq_send_times[si->responses] = seq_send_times[i];
       }
       if (si->responses > 0) {
	 seq_diffs[si->responses - 1] = MOD_DIFF(si->seqs[si->responses], si->seqs[si->responses - 1]);
	 ts_diffs[si->responses - 1] = MOD_DIFF(si->timestamps[si->responses], si->timestamps[si->responses - 1]);
	 time_usec_diffs[si->responses - 1] = TIMEVAL_SUBTRACT(seq_send_times[si->responses], seq_send_times[si->responses - 1]);
	 if (!time_usec_diffs[si->responses - 1]) time_usec_diffs[si->responses - 1]++; /* We divide by this later */
	 /*	 printf("MOD_DIFF_USHORT(%hu, %hu) == %hu\n", si->ipids[si->responses], si->ipids[si->responses - 1], MOD_DIFF_USHORT(si->ipids[si->responses], si->ipids[si->responses - 1])); */
       }

       si->responses++;
     } /* Otherwise nothing good in this slot to copy */
   }
     

   si->ipid_seqclass = ipid_sequence(si->responses, si->ipids, 
				     islocalhost(target->v4hostip()));

   /* Now we look at TCP Timestamp sequence prediction */
   /* Battle plan:
      1) Compute average increments per second, and variance in incr. per second 
      2) If any are 0, set to constant
      3) If variance is high, set to random incr. [ skip for now ]
      4) if ~10/second, set to appropriate thing
      5) Same with ~100/sec
   */
   if (si->ts_seqclass == TS_SEQ_UNKNOWN && si->responses >= 2) {
     avg_ts_hz = 0.0;
     for(i=0; i < si->responses - 1; i++) {
       double dhz;

       dhz = (double) ts_diffs[i] / (time_usec_diffs[i] / 1000000.0);
       /*       printf("ts incremented by %d in %li usec -- %fHZ\n", ts_diffs[i], time_usec_diffs[i], dhz); */
       avg_ts_hz += dhz / ( si->responses - 1);
     }

     if (o.debugging)
       printf("The avg TCP TS HZ is: %f\n", avg_ts_hz);
     
     if (avg_ts_hz > 0 && avg_ts_hz < 3.9) { /* relatively wide range because sampling time so short and frequency so slow */
       si->ts_seqclass = TS_SEQ_2HZ;
       si->lastboot = seq_send_times[0].tv_sec - (si->timestamps[0] / 2); 
     }
     else if (avg_ts_hz > 85 && avg_ts_hz < 115) {
       si->ts_seqclass = TS_SEQ_100HZ;
       si->lastboot = seq_send_times[0].tv_sec - (si->timestamps[0] / 100); 
     }
     else if (avg_ts_hz > 900 && avg_ts_hz < 1100) {
       si->ts_seqclass = TS_SEQ_1000HZ;
       si->lastboot = seq_send_times[0].tv_sec - (si->timestamps[0] / 1000); 
     }
     if (si->lastboot && (seq_send_times[0].tv_sec - si->lastboot > 63072000))
       {
	 /* Up 2 years?  Perhaps, but they're probably lying. */
	 if (o.debugging) {
	   error("Ignoring claimed uptime of %lu days", 
		 (seq_send_times[0].tv_sec - si->lastboot) / 86400);
	 }
	 si->lastboot = 0;
       }
   }
   
   /* Time to look at the TCP ISN predictability */
   if (si->responses >= 4 && o.scan_delay <= 1000) {
     seq_gcd = gcd_n_uint(si->responses -1, seq_diffs);
     /*     printf("The GCD is %u\n", seq_gcd);*/
     if (seq_gcd) {     
       for(i=0; i < si->responses - 1; i++)
	 seq_diffs[i] /= seq_gcd;
       for(i=0; i < si->responses - 1; i++) {     
	 if (MOD_DIFF(si->seqs[i+1],si->seqs[i]) > 50000000) {
	   si->seqclass = SEQ_TR;
	   si->index = 9999999;
	   /*	 printf("Target is a TR box\n");*/
	   break;
	 }	
	 seq_avg_inc += seq_diffs[i];
       }
     }
     if (seq_gcd == 0) {
       si->seqclass = SEQ_CONSTANT;
       si->index = 0;
     } else if (seq_gcd % 64000 == 0) {
       si->seqclass = SEQ_64K;
       /*       printf("Target is a 64K box\n");*/
       si->index = 1;
     } else if (seq_gcd % 800 == 0) {
       si->seqclass = SEQ_i800;
       /*       printf("Target is a i800 box\n");*/
       si->index = 10;
     } else if (si->seqclass == SEQ_UNKNOWN) {
       seq_avg_inc = (unsigned int) ((0.5) + seq_avg_inc / (si->responses - 1));
       /*       printf("seq_avg_inc=%u\n", seq_avg_inc);*/
       for(i=0; i < si->responses -1; i++)       {     

	 /*	 printf("The difference is %u\n", seq_diffs[i]);
		 printf("Adding %u^2=%e", MOD_DIFF(seq_diffs[i], seq_avg_inc), pow(MOD_DIFF(seq_diffs[i], seq_avg_inc), 2));*/
	 /* pow() seems F#@!#$!ed up on some Linux systems so I will
	    not use it for now 
  	    seq_inc_sum += pow(MOD_DIFF(seq_diffs[i], seq_avg_inc), 2);
	 */	 
	 
	 seq_inc_sum += ((double)(MOD_DIFF(seq_diffs[i], seq_avg_inc)) * ((double)MOD_DIFF(seq_diffs[i], seq_avg_inc)));
	 /*	 seq_inc_sum += pow(MOD_DIFF(seq_diffs[i], seq_avg_inc), 2);*/

       }
       /*       printf("The sequence sum is %e\n", seq_inc_sum);*/
       seq_inc_sum /= (si->responses - 1);

       /* Some versions of Linux libc seem to have broken pow ... so we
	  avoid it */
#ifdef LINUX       
       si->index = (unsigned int) (0.5 + sqrt(seq_inc_sum));
#else
       si->index = (unsigned int) (0.5 + pow(seq_inc_sum, 0.5));
#endif

       /*       printf("The sequence index is %d\n", si->index);*/
       if (si->index < 75) {
	 si->seqclass = SEQ_TD;
	 /*	 printf("Target is a Micro$oft style time dependant box\n");*/
       }
       else {
	 si->seqclass = SEQ_RI;
	 /*	 printf("Target is a random incremental box\n");*/
       }
     }
     FPtests[0] = (FingerPrint *) safe_zalloc(sizeof(FingerPrint));
     FPtests[0]->name = "TSeq";
     seq_AVs = (struct AVal *) safe_zalloc(sizeof(struct AVal) * 5);
     FPtests[0]->results = seq_AVs;
     avnum = 0;
     seq_AVs[avnum].attribute = "Class";
     switch(si->seqclass) {
     case SEQ_CONSTANT:
       strcpy(seq_AVs[avnum].value, "C");
       seq_AVs[avnum].next = &seq_AVs[avnum+1]; avnum++;
       seq_AVs[avnum].attribute= "Val";     
       sprintf(seq_AVs[avnum].value, "%X", si->seqs[0]);
       break;
     case SEQ_64K:
       strcpy(seq_AVs[avnum].value, "64K");      
       break;
     case SEQ_i800:
       strcpy(seq_AVs[avnum].value, "i800");
       break;
     case SEQ_TD:
       strcpy(seq_AVs[avnum].value, "TD");
       seq_AVs[avnum].next = &seq_AVs[avnum+1]; avnum++;
       seq_AVs[avnum].attribute= "gcd";     
       sprintf(seq_AVs[avnum].value, "%X", seq_gcd);
       seq_AVs[avnum].next = &seq_AVs[avnum+1]; avnum++;
       seq_AVs[avnum].attribute="SI";
       sprintf(seq_AVs[avnum].value, "%X", si->index);
       break;
     case SEQ_RI:
       strcpy(seq_AVs[avnum].value, "RI");
       seq_AVs[avnum].next = &seq_AVs[avnum+1]; avnum++;
       seq_AVs[avnum].attribute= "gcd";     
       sprintf(seq_AVs[avnum].value, "%X", seq_gcd);
       seq_AVs[avnum].next = &seq_AVs[avnum+1]; avnum++;
       seq_AVs[avnum].attribute="SI";
       sprintf(seq_AVs[avnum].value, "%X", si->index);
       break;
     case SEQ_TR:
       strcpy(seq_AVs[avnum].value, "TR");
       break;
     }

     /* IP ID Class */
     switch(si->ipid_seqclass) {
     case IPID_SEQ_CONSTANT:
       seq_AVs[avnum].next = &seq_AVs[avnum+1]; avnum++;
       seq_AVs[avnum].attribute = "IPID";
       strcpy(seq_AVs[avnum].value, "C");
       break;
     case IPID_SEQ_INCR:
       seq_AVs[avnum].next = &seq_AVs[avnum+1]; avnum++;
       seq_AVs[avnum].attribute = "IPID";
       strcpy(seq_AVs[avnum].value, "I");
       break;
     case IPID_SEQ_BROKEN_INCR:
       seq_AVs[avnum].next = &seq_AVs[avnum+1]; avnum++;
       seq_AVs[avnum].attribute = "IPID";
       strcpy(seq_AVs[avnum].value, "BI");
       break;
     case IPID_SEQ_RPI:
       seq_AVs[avnum].next = &seq_AVs[avnum+1]; avnum++;
       seq_AVs[avnum].attribute = "IPID";
       strcpy(seq_AVs[avnum].value, "RPI");
       break;
     case IPID_SEQ_RD:
       seq_AVs[avnum].next = &seq_AVs[avnum+1]; avnum++;
       seq_AVs[avnum].attribute = "IPID";
       strcpy(seq_AVs[avnum].value, "RD");
       break;
     case IPID_SEQ_ZERO:
       seq_AVs[avnum].next = &seq_AVs[avnum+1]; avnum++;
       seq_AVs[avnum].attribute = "IPID";
       strcpy(seq_AVs[avnum].value, "Z");
       break;
     }

     /* TCP Timestamp option sequencing */
     switch(si->ts_seqclass) {
     case TS_SEQ_ZERO:
       seq_AVs[avnum].next = &seq_AVs[avnum+1]; avnum++;
       seq_AVs[avnum].attribute = "TS";
       strcpy(seq_AVs[avnum].value, "0");
       break;
     case TS_SEQ_2HZ:
       seq_AVs[avnum].next = &seq_AVs[avnum+1]; avnum++;
       seq_AVs[avnum].attribute = "TS";
       strcpy(seq_AVs[avnum].value, "2HZ");
       break;
     case TS_SEQ_100HZ:
       seq_AVs[avnum].next = &seq_AVs[avnum+1]; avnum++;
       seq_AVs[avnum].attribute = "TS";
       strcpy(seq_AVs[avnum].value, "100HZ");
       break;
     case TS_SEQ_1000HZ:
       seq_AVs[avnum].next = &seq_AVs[avnum+1]; avnum++;
       seq_AVs[avnum].attribute = "TS";
       strcpy(seq_AVs[avnum].value, "1000HZ");
       break;
     case TS_SEQ_UNSUPPORTED:
       seq_AVs[avnum].next = &seq_AVs[avnum+1]; avnum++;
       seq_AVs[avnum].attribute = "TS";
       strcpy(seq_AVs[avnum].value, "U");
       break;
     }
   }
   else {
     log_write(LOG_STDOUT|LOG_NORMAL|LOG_SKID,"Insufficient responses for TCP sequencing (%d), OS detection may be less accurate\n", si->responses);
   }
 } else {
 }

for(i=0; i < 9; i++) {
  if (i > 0 && !FPtests[i] && ((openport != (unsigned long) -1) || i > 4)) {
    /* We create a Resp (response) attribute with value of N (no) because
       it is important here to note whether responses were or were not 
       received */
    FPtests[i] = (FingerPrint *) safe_zalloc(sizeof(FingerPrint));
    seq_AVs = (struct AVal *) safe_zalloc(sizeof(struct AVal));
    seq_AVs->attribute = "Resp";
    strcpy(seq_AVs->value, "N");
    seq_AVs->next = NULL;
    FPtests[i]->results = seq_AVs;
    FPtests[i]->name =  (i == 1)? "T1" : (i == 2)? "T2" : (i == 3)? "T3" : (i == 4)? "T4" : (i == 5)? "T5" : (i == 6)? "T6" : (i == 7)? "T7" : "PU";
  }
}
 last = -1;
 FP = NULL;
 for(i=0; i < 9 ; i++) {
   if (!FPtests[i]) continue; 
   if (!FP) FP = FPtests[i];
   if (last > -1) {
     FPtests[last]->next = FPtests[i];    
   }
   last = i;
 }
 if (last) FPtests[last]->next = NULL;
 
 osscan_timedout:
 if (target->timedout)
   FP = NULL;
 close(rawsd);
 pcap_close(pd);
 return FP;
}


struct AVal *fingerprint_iptcppacket(struct ip *ip, int mss, u32 syn) {
  struct AVal *AVs;
  int length;
  int opcode;
  u16 tmpshort;
  char *p,*q;
  struct tcphdr *tcp = ((struct tcphdr *) (((char *) ip) + 4 * ip->ip_hl));

  AVs = (struct AVal *) malloc(6 * sizeof(struct AVal));

  /* Link them together */
  AVs[0].next = &AVs[1];
  AVs[1].next = &AVs[2];
  AVs[2].next = &AVs[3];
  AVs[3].next = &AVs[4];
  AVs[4].next = &AVs[5];
  AVs[5].next = NULL;

  /* First we give the "response" flag to say we did actually receive
     a packet -- this way we won't match a template with Resp=N */
  AVs[0].attribute = "Resp";
  strcpy(AVs[0].value, "Y");


  /* Next we check whether the Don't Fragment bit is set */
  AVs[1].attribute = "DF";
  if(ntohs(ip->ip_off) & 0x4000) {
    strcpy(AVs[1].value,"Y");
  } else strcpy(AVs[1].value, "N");

  /* Now we do the TCP Window size */
  AVs[2].attribute = "W";
  sprintf(AVs[2].value, "%hX", ntohs(tcp->th_win));

  /* Time for the ACK, the codes are:
     S   = same as syn
     S++ = syn + 1
     O   = other
  */
  AVs[3].attribute = "ACK";
  if (ntohl(tcp->th_ack) == syn + 1)
    strcpy(AVs[3].value, "S++");
  else if (ntohl(tcp->th_ack) == syn) 
    strcpy(AVs[3].value, "S");
  else strcpy(AVs[3].value, "O");
    
  /* Now time for the flags ... they must be in this order:
     B = Bogus (64, not a real TCP flag)
     U = Urgent
     A = Acknowledgement
     P = Push
     R = Reset
     S = Synchronize
     F = Final
  */
  AVs[4].attribute = "Flags";
  p = AVs[4].value;
  if (tcp->th_flags & TH_ECE) *p++ = 'B';
  if (tcp->th_flags & TH_URG) *p++ = 'U';
  if (tcp->th_flags & TH_ACK) *p++ = 'A';
  if (tcp->th_flags & TH_PUSH) *p++ = 'P';
  if (tcp->th_flags & TH_RST) *p++ = 'R';
  if (tcp->th_flags & TH_SYN) *p++ = 'S';
  if (tcp->th_flags & TH_FIN) *p++ = 'F';
  *p++ = '\0';

  /* Now for the TCP options ... */
  AVs[5].attribute = "Ops";
  p = AVs[5].value;
  /* Partly swiped from /usr/src/linux/net/ipv4/tcp_input.c in Linux kernel */
  length = (tcp->th_off * 4) - sizeof(struct tcphdr);
  q = ((char *)tcp) + sizeof(struct tcphdr);

  while(length > 0 &&
	((p - AVs[5].value) < (int) (sizeof(AVs[5].value) - 3))) {
    opcode=*q++;
    length--;
    if (!opcode) {
      *p++ = 'L'; /* End of List */
      break;
    } else if (opcode == 1) {
      *p++ = 'N'; /* No Op */
    } else if (opcode == 2) {
      *p++ = 'M'; /* MSS */
      q++;
      memcpy(&tmpshort, q, 2);
      if(ntohs(tmpshort) == mss)
	*p++ = 'E'; /* Echoed */
      q += 2;
      length -= 3;
    } else if (opcode == 3) { /* Window Scale */
      *p++ = 'W';
      q += 2;
      length -= 2;
    } else if (opcode == 8) { /* Timestamp */
      *p++ = 'T';
      q += 9;
      length -= 9;
    }
  }
  *p++ = '\0';
  return AVs;
}

// Prints a note if observedFP has a classification and it is not in referenceFP
// Returns 0 if they match, nonzero otherwise
static int compareclassifications(FingerPrint *referenceFP, 
				  FingerPrint *observedFP, bool verbose) {
  int refclassno;
  struct OS_Classification *obclass, *refclass;
  if (observedFP->num_OS_Classifications > 0) {
    obclass = &(observedFP->OS_class[0]);
    for(refclassno = 0; refclassno < referenceFP->num_OS_Classifications; refclassno++) {
      refclass = &(referenceFP->OS_class[refclassno]);
      if (strcmp(obclass->OS_Vendor, refclass->OS_Vendor) == 0 &&
	  strcmp(obclass->OS_Family, refclass->OS_Family) == 0 &&
	  strcmp(obclass->Device_Type, refclass->Device_Type) == 0 &&
	  (obclass->OS_Generation == refclass->OS_Generation ||
	   (obclass->OS_Generation != NULL && refclass->OS_Generation != NULL && 
	    strcmp(obclass->OS_Generation, refclass->OS_Generation) == 0))) {
	// A match!  lets get out of here
	return 0;
      }
    }
  } else {
    if (verbose)
      printf("Observed fingerprint lacks a classification\n");
    return 1;
  }
  if (verbose)
    printf("Warning: Classification of observed fingerprint does not appear in reference fingerprint.\n");
  return 1;
}

/* Compares 2 fingerprints -- a referenceFP (can have expression
   attributes) with an observed fingerprint (no expressions).  If
   verbose is nonzero, differences will be printed.  The comparison
   accuracy (between 0 and 1) is returned). */
double compare_fingerprints(FingerPrint *referenceFP, FingerPrint *observedFP,
			    int verbose) {
  FingerPrint *currentReferenceTest;
  struct AVal *currentObservedTest;
  unsigned long num_subtests = 0, num_subtests_succeeded = 0;
  unsigned long  new_subtests, new_subtests_succeeded;
  assert(referenceFP);
  assert(observedFP);

  if (verbose) compareclassifications(referenceFP, observedFP, true);

  for(currentReferenceTest = referenceFP; currentReferenceTest; 
      currentReferenceTest = currentReferenceTest->next) {
    currentObservedTest = gettestbyname(observedFP, currentReferenceTest->name);
    if (currentObservedTest) {
      new_subtests = new_subtests_succeeded = 0;
      AVal_match(currentReferenceTest->results, currentObservedTest, 
		 &new_subtests, &new_subtests_succeeded, 0);
      if (verbose && new_subtests_succeeded < new_subtests) 
	printf("Test %s differs in %li attributes\n", 
	       currentReferenceTest->name, 
	       new_subtests - new_subtests_succeeded);      
      num_subtests += new_subtests;
      num_subtests_succeeded += new_subtests_succeeded;
    }
  }

  assert(num_subtests_succeeded <= num_subtests);
  return (num_subtests)? (num_subtests_succeeded / (double) num_subtests) : 0; 
}

/* Takes a fingerprint and looks for matches inside reference_FPs[].
   The results are stored in in FPR (which must point to an instantiated
   FingerPrintResults class) -- results will be reverse-sorted by
   accuracy.  No results below accuracy_threshhold will be included.
   The max matches returned is the maximum that fits in a
   FingerPrintResults class.  */
void match_fingerprint(FingerPrint *FP, FingerPrintResults *FPR, 
		      FingerPrint **reference_FPs, double accuracy_threshold) {

  int i;
  double FPR_entrance_requirement = accuracy_threshold; /* accuracy must be 
							   at least this big 
							   to be added to the 
							   list */
  FingerPrint *current_os;
  double acc;
  int state;
  int skipfp;
  int max_prints = sizeof(FPR->prints) / sizeof(FingerPrint *);
  int idx;
  double tmp_acc=0.0, tmp_acc2; /* These are temp buffers for list swaps */
  FingerPrint *tmp_FP=NULL, *tmp_FP2;

  assert(FP);
  assert(FPR);
  assert(accuracy_threshold >= 0 && accuracy_threshold <= 1);

  FPR->overall_results = OSSCAN_SUCCESS;
  
  for(i = 0; reference_FPs[i]; i++) {
    current_os = reference_FPs[i];
    skipfp = 0;

    acc = compare_fingerprints(current_os, FP, 0);

    /*    error("Comp to %s: %li/%li=%f", o.reference_FPs[i]->OS_name, num_subtests_succeeded, num_subtests, acc); */
    if (acc >= FPR_entrance_requirement || acc == 1.0) {

      state = 0;
      for(idx=0; idx < FPR->num_matches; idx++) {	
	if (strcmp(FPR->prints[idx]->OS_name, current_os->OS_name) == 0) {
	  if (FPR->accuracy[idx] >= acc) {
	    skipfp = 1; /* Skip it -- a higher version is already in list */
	  } else {	  
	    /* We must shift the list left to delete this sucker */
	    memmove(FPR->prints + idx, FPR->prints + idx + 1,
		    (FPR->num_matches - 1 - idx) * sizeof(FingerPrint *));
	    memmove(FPR->accuracy + idx, FPR->accuracy + idx + 1,
		    (FPR->num_matches - 1 - idx) * sizeof(double));
	    FPR->num_matches--;
	    FPR->accuracy[FPR->num_matches] = 0;
	  }
	  break; /* There can only be 1 in the list with same name */
	}
      }

      if (!skipfp) {      
	/* First we check whether we have overflowed with perfect matches */
	if (acc == 1) {
	  /*	  error("DEBUG: Perfect match #%d/%d", FPR->num_perfect_matches + 1, max_prints); */
	  if (FPR->num_perfect_matches == max_prints) {
	    FPR->overall_results = OSSCAN_TOOMANYMATCHES;
	    return;
	  }
	  FPR->num_perfect_matches++;
	}
	
	/* Now we add the sucker to the list */
	state = 0; /* Have not yet done the insertion */
	for(idx=-1; idx < max_prints -1; idx++) {
	  if (state == 1) {
	    /* Push tmp_acc and tmp_FP onto the next idx */
	    tmp_acc2 = FPR->accuracy[idx+1];
	    tmp_FP2 = FPR->prints[idx+1];
	    
	    FPR->accuracy[idx+1] = tmp_acc;
	    FPR->prints[idx+1] = tmp_FP;
	    
	    tmp_acc = tmp_acc2;
	    tmp_FP = tmp_FP2;
	  } else if (FPR->accuracy[idx + 1] < acc) {
	    /* OK, I insert the sucker into the next slot ... */
	    tmp_acc = FPR->accuracy[idx+1];
	    tmp_FP = FPR->prints[idx+1];
	    FPR->prints[idx+1] = current_os;
	    FPR->accuracy[idx+1] = acc;
	    state = 1;
	  }
	}
	if (state != 1) {
	  fatal("Bogus list insertion state (%d) -- num_matches = %d num_perfect_matches=%d entrance_requirement=%f", state, FPR->num_matches, FPR->num_perfect_matches, FPR_entrance_requirement);
	}
	FPR->num_matches++;
	/* If we are over max_prints, one was shoved off list */
	if (FPR->num_matches > max_prints) FPR->num_matches = max_prints;

	/* Calculate the new min req. */
	if (FPR->num_matches == max_prints) {
	  FPR_entrance_requirement = FPR->accuracy[max_prints - 1] + 0.00001;
	}
      }
    }
  }

  if (FPR->num_matches == 0 && FPR->overall_results == OSSCAN_SUCCESS)
    FPR->overall_results = OSSCAN_NOMATCHES;

  return;
}

struct AVal *gettestbyname(FingerPrint *FP, const char *name) {

  if (!FP) return NULL;
  do {
    if (!strcmp(FP->name, name))
      return FP->results;
    FP = FP->next;
  } while(FP);
  return NULL;
}

struct AVal *getattrbyname(struct AVal *AV, const char *name) {

  if (!AV) return NULL;
  do {
    if (!strcmp(AV->attribute, name))
      return AV;
    AV = AV->next;
  } while(AV);
  return NULL;
}


/* Returns true if perfect match -- if num_subtests &
   num_subtests_succeeded are non_null it ADDS THE NEW VALUES to what
   is already there.  So initialize them to zero first if you only
   want to see the results from this match.  if shortcircuit is zero,
   it does all the tests, otherwise it returns when the first one
   fails. */
int AVal_match(struct AVal *reference, struct AVal *fprint, unsigned long *num_subtests, unsigned long *num_subtests_succeeded, int shortcut) {
  struct AVal *current_ref;
  struct AVal *current_fp;
  unsigned int number;
  unsigned int val;
  char *p, *q;  /* OHHHH YEEEAAAAAHHHH!#!@#$!% */
  char valcpy[512];
  char *endptr;
  int andexp, orexp, expchar, numtrue;
  int testfailed;
  int subtests = 0, subtests_succeeded=0;

  for(current_ref = reference; current_ref; current_ref = current_ref->next) {
    current_fp = getattrbyname(fprint, current_ref->attribute);    
    if (!current_fp) continue;
    /* OK, we compare an attribute value in  current_fp->value to a 
       potentially large expression in current_ref->value.  The syntax uses
       < (less than), > (greather than), + (non-zero), | (or), and & (and) 
       No parenthesis are allowed and an expression cannot have | AND & */
    numtrue = andexp = orexp = 0; testfailed = 0;
    Strncpy(valcpy, current_ref->value, sizeof(valcpy));
    p = valcpy;
    if (strchr(current_ref->value, '|')) {
      orexp = 1; expchar = '|';
    } else {
      andexp = 1; expchar = '&';
    }
    do {
      q = strchr(p, expchar);
      if (q) *q = '\0';
      if (strcmp(p, "+") == 0) {
	if (!*current_fp->value) { if (andexp) { testfailed=1; break; } }
	else {
	  val = strtol(current_fp->value, &endptr, 16);
	  if (val == 0 || *endptr) { if (andexp) { testfailed=1; break; } }
	  else { numtrue++; if (orexp) break; }
	}
      } else if (*p == '<' && isxdigit((int) p[1])) {
	if (!*current_fp->value) { if (andexp) { testfailed=1; break; } }
	number = strtol(p + 1, &endptr, 16);
	val = strtol(current_fp->value, &endptr, 16);
	if (val >= number || *endptr) { if (andexp)  { testfailed=1; break; } }
	else { numtrue++; if (orexp) break; }
      } else if (*p == '>' && isxdigit((int) p[1])) {
	if (!*current_fp->value) { if (andexp) { testfailed=1; break; } }
	number = strtol(p + 1, &endptr, 16);
	val = strtol(current_fp->value, &endptr, 16);
	if (val <= number || *endptr) { if (andexp) { testfailed=1; break; } }
	else { numtrue++; if (orexp) break; }
      }
      else {
	if (strcmp(p, current_fp->value))
	  { if (andexp) { testfailed=1; break; } }
	else { numtrue++; if (orexp) break; }
      }
      if (q) p = q + 1;
    } while(q);
      if (numtrue == 0) testfailed=1;
      subtests++;
      if (testfailed) {
	if (shortcut) {
	  if (num_subtests) *num_subtests += subtests;
	  return 0;
	}
      } else subtests_succeeded++;
      /* Whew, we made it past one Attribute alive , on to the next! */
  }
  if (num_subtests) *num_subtests += subtests;
  if (num_subtests_succeeded) *num_subtests_succeeded += subtests_succeeded;
  return (subtests == subtests_succeeded)? 1 : 0;
}


void freeFingerPrint(FingerPrint *FP) {
FingerPrint *currentFP;
FingerPrint *nextFP;

if (!FP) return;

 for(currentFP = FP; currentFP; currentFP = nextFP) {
   nextFP = currentFP->next;
   if (currentFP->results)
     free(currentFP->results);
   free(currentFP);
 }
return;
}


int os_scan(Target *target) {

FingerPrintResults FP_matches[3];
struct seq_info si[3];
int itry;
int i;
struct timeval now;
double bestacc;
int bestaccidx;

 if (target->timedout)
   return 1;
 
 if (target->FPR == NULL)
   target->FPR = new FingerPrintResults;

 memset(si, 0, sizeof(si));
 if (target->ports.state_counts_tcp[PORT_OPEN] == 0 ||
     (target->ports.state_counts_tcp[PORT_CLOSED] == 0 &&
      target->ports.state_counts_tcp[PORT_UNFIREWALLED] == 0)) {
   if (o.osscan_limit) {
     if (o.verbose)
       log_write(LOG_STDOUT|LOG_NORMAL|LOG_SKID, "Skipping OS Scan due to absence of open (or perhaps closed) ports\n");
     return 1;
   } else {   
     log_write(LOG_STDOUT|LOG_NORMAL|LOG_SKID,"Warning:  OS detection will be MUCH less reliable because we did not find at least 1 open and 1 closed TCP port\n");
   }
 }

 for(itry=0; itry < 3; itry++) {
   if (o.host_timeout) {   
     gettimeofday(&now, NULL);
     if (target->timedout || TIMEVAL_MSEC_SUBTRACT(now, target->host_timeout) >= 0)
       {
	 target->timedout = 1;
	 return 1;
       }
   }
   target->FPR->FPs[itry] = get_fingerprint(target, &si[itry]); 
   if (target->timedout)
     return 1;
   match_fingerprint(target->FPR->FPs[itry], &FP_matches[itry], 
		     o.reference_FPs, OSSCAN_GUESS_THRESHOLD);
   if (FP_matches[itry].overall_results == OSSCAN_SUCCESS && 
       FP_matches[itry].num_perfect_matches > 0)
     break;
   if (itry < 2)
     sleep(2);
 }

 target->FPR->numFPs = (itry == 3)? 3 : itry + 1;
 memcpy(&(target->seq), &si[target->FPR->numFPs - 1], sizeof(struct seq_info));

 /* Now lets find the best match */
 bestacc = 0;
 bestaccidx = 0;
 for(itry=0; itry < target->FPR->numFPs; itry++) {
   if (FP_matches[itry].overall_results == OSSCAN_SUCCESS &&
       FP_matches[itry].num_matches > 0 &&
       FP_matches[itry].accuracy[0] > bestacc) {
     bestacc = FP_matches[itry].accuracy[0];
     bestaccidx = itry;
     if (FP_matches[itry].num_perfect_matches)
       break;
   }
 }


 for(i=0; i < target->FPR->numFPs; i++) {
   if (i == bestaccidx)
     continue;
   if (o.debugging) {
     error("Failed exact match #%d (0-based):\n%s", i, fp2ascii(target->FPR->FPs[i]));
   }
 }

 if (target->FPR->numFPs > 1 && target->FPR->overall_results == OSSCAN_SUCCESS &&
     target->FPR->accuracy[0] == 1.0) {
   if (o.verbose) error("WARNING:  OS didn't match until the try #%d", target->FPR->numFPs);
 } 

 target->FPR->goodFP = bestaccidx;

 // Now we redo the match, since target->FPR has various data (such as
 // target->FPR->numFPs) which is not in FP_matches[bestaccidx].  This is
 // kinda ugly.
 if (target->FPR->goodFP >= 0)
   match_fingerprint(target->FPR->FPs[target->FPR->goodFP], target->FPR, 
		     o.reference_FPs, OSSCAN_GUESS_THRESHOLD);

 
 return 1;
}

/* Writes an informational "Test" result suitable for including at the
   top of a fingerprint.  Gives info which might be useful when the
   FPrint is submitted (eg Nmap version, etc).  Result is written (up
   to ostrlen) to the ostr var passed in */
void WriteSInfo(char *ostr, int ostrlen, int openport, int closedport) {
  struct tm *ltime;
  time_t timep;
  
  timep = time(NULL);
  ltime = localtime(&timep);

  snprintf(ostr, ostrlen, "SInfo(V=%s%%P=%s%%D=%d/%d%%Time=%X%%O=%d%%C=%d)\n", 
	   NMAP_VERSION, NMAP_PLATFORM, ltime->tm_mon + 1, ltime->tm_mday, 
	   (int) timep, openport, closedport);
}

char *mergeFPs(FingerPrint *FPs[], int numFPs, int openport, int closedport) {
static char str[10240];
struct AVal *AV;
FingerPrint *currentFPs[32];
char *p = str;
int i;
int changed;
char *end = str + sizeof(str) - 1; /* Last byte allowed to write into */
if (numFPs <=0) return "(None)";
if (numFPs > 32) return "(Too many)";
  
memset(str, 0, sizeof(str));
for(i=0; i < numFPs; i++) {
  if (FPs[i] == NULL) {
    fatal("mergeFPs was handed a pointer to null fingerprint");
  }
  currentFPs[i] = FPs[i];
}

/* Lets start by writing the fake "Info" test for submitting fingerprints */
 WriteSInfo(str, sizeof(str), openport, closedport); 
 p = p + strlen(str);

do {
  changed = 0;
  for(i = 0; i < numFPs; i++) {
    assert (end - p > 100);
    if (currentFPs[i]) {
      /* This junk means do not print this one if the next
	 one is the same */
      if (i == numFPs - 1 || !currentFPs[i+1] ||
	  strcmp(currentFPs[i]->name, currentFPs[i+1]->name) != 0 ||
	  AVal_match(currentFPs[i]->results,currentFPs[i+1]->results, NULL,
		     NULL, 1) ==0)
	{
	  changed = 1;
	  Strncpy(p, currentFPs[i]->name, end - p);
	  p += strlen(currentFPs[i]->name);
	  *p++='(';
	  for(AV = currentFPs[i]->results; AV; AV = AV->next) {
	    Strncpy(p, AV->attribute, end - p);
	    p += strlen(AV->attribute);
	    *p++='=';
	    Strncpy(p, AV->value, end - p);
	    p += strlen(AV->value);
	    *p++ = '%';
	  }
	  if(*(p-1) != '(')
	    p--; /* Kill the final & */
	  *p++ = ')';
	  *p++ = '\n';
	}
      /* Now prepare for the next one */
      currentFPs[i] = currentFPs[i]->next;
    }
  }
} while(changed);

*p = '\0';
return str;
}


char *fp2ascii(FingerPrint *FP) {
static char str[2048];
FingerPrint *current;
struct AVal *AV;
char *p = str;
int len;
memset(str, 0, sizeof(str));

if (!FP) return "(None)";

if(FP->OS_name && *(FP->OS_name)) {
  len = snprintf(str, 128, "FingerPrint  %s\n", FP->OS_name);
  if (len < 0) fatal("OS name too long");
  p += len;
}

for(current = FP; current ; current = current->next) {
  Strncpy(p, current->name, sizeof(str) - (p-str));
  p += strlen(p);
  assert(p-str < (int) sizeof(str) - 30);
  *p++='(';
  for(AV = current->results; AV; AV = AV->next) {
    Strncpy(p, AV->attribute, sizeof(str) - (p-str));
    p += strlen(p);
    assert(p-str < (int) sizeof(str) - 30);
    *p++='=';
    Strncpy(p, AV->value, sizeof(str) - (p-str));
    p += strlen(p);
    assert(p-str < (int) sizeof(str) - 30);
    *p++ = '%';
  }
  if(*(p-1) != '(')
    p--; /* Kill the final & */
  *p++ = ')';
  *p++ = '\n';
}
*p = '\0';
return str;
}

/* Parse a 'Class' line found in the fingerprint file into the current
   FP.  Classno is the number of 'class' lines found so far in the
   current fingerprint.  The function quits if there is a parse error */

void parse_classline(FingerPrint *FP, char *thisline, int lineno, int *classno) {
  char *p, *q;

  fflush(stdout);

  if (!thisline || strncmp(thisline, "Class ", 6) == 1) {
    fatal("Bogus line #%d (%s) passed to parse_classline()", lineno, thisline);
  }

  if (*classno >= MAX_OS_CLASSIFICATIONS_PER_FP)
    fatal("Too many Class lines in fingerprint (line %d: %s), remove some or increase MAX_OS_CLASSIFICATIONS_PER_FP", lineno, thisline);
  
  p = thisline + 6;
  
  /* First lets get the vendor name */
  while(*p && isspace(*p)) p++;
  
  q = strchr(p, '|');
  if (!q) {
    fatal("Parse error on line %d of fingerprint: %s\n", lineno, thisline);
  }
  
  // Trim any trailing whitespace
  q--;
  while(isspace(*q)) q--;
  q++;
  if (q < p) { fatal("Parse error on line %d of fingerprint: %s\n", lineno, thisline); }
  FP->OS_class[*classno].OS_Vendor = (char *) cp_alloc(q - p + 1);
  memcpy(FP->OS_class[*classno].OS_Vendor, p, q - p);
  FP->OS_class[*classno].OS_Vendor[q - p] = '\0';
  
  /* Next comes the OS Family */
  p = q;
  while(*p && !isalnum(*p)) p++;

  q = strchr(p, '|');
  if (!q) {
    fatal("Parse error on line %d of fingerprint: %s\n", lineno, thisline);
  }
  // Trim any trailing whitespace
  q--;
  while(isspace(*q)) q--;
  q++;
  if (q < p) { fatal("Parse error on line %d of fingerprint: %s\n", lineno, thisline); }
  FP->OS_class[*classno].OS_Family = (char *) cp_alloc(q - p + 1);
  memcpy(FP->OS_class[*classno].OS_Family, p, q - p);
  FP->OS_class[*classno].OS_Family[q - p] = '\0';
  
  /* And now the the OS generation, if available */
  p = q;
  while(*p && *p != '|') p++;
  if (*p) p++;
  while(*p && isspace(*p) && *p != '|') p++;
  if (*p == '|') {
    FP->OS_class[*classno].OS_Generation = NULL;
    q = p;
  }
  else {
    q = strpbrk(p, " |");
    if (!q) {
      fatal("Parse error on line %d of fingerprint: %s\n", lineno, thisline);
    }
    // Trim any trailing whitespace
    q--;
    while(isspace(*q)) q--;
    q++;
    if (q < p) { fatal("Parse error on line %d of fingerprint: %s\n", lineno, thisline); }
    FP->OS_class[*classno].OS_Generation = (char *) cp_alloc(q - p + 1);
    memcpy(FP->OS_class[*classno].OS_Generation, p, q - p);
    FP->OS_class[*classno].OS_Generation[q - p] = '\0';
  }
  
  /* And finally the device type */
  p = q;
  while(*p && !isalnum(*p)) p++;
  
  q = strchr(p, '|');
  if (!q) {
    q = p;
    while(*q) q++;
  }
  
  // Trim any trailing whitespace
  q--;
  while(isspace(*q)) q--;
  q++;
  if (q < p) { fatal("Parse error on line %d of fingerprint: %s\n", lineno, thisline); }
  FP->OS_class[*classno].Device_Type = (char *) cp_alloc(q - p + 1);
  memcpy(FP->OS_class[*classno].Device_Type, p, q - p);
  FP->OS_class[*classno].Device_Type[q - p] = '\0';
  

  //  printf("Got classification #%d for the OS %s: VFGT: %s * %s * %s * %s\n", *classno, FP->OS_name, FP->OS_class[*classno].OS_Vendor, FP->OS_class[*classno].OS_Family, FP->OS_class[*classno].OS_Generation? FP->OS_class[*classno].OS_Generation : "(null)", FP->OS_class[*classno].Device_Type);

  (*classno)++;
  FP->num_OS_Classifications++;

}

/* Parses a single fingerprint from the memory region given.  If a
 non-null fingerprint is returned, the user is in charge of freeing it
 when done.  This function does not require the fingerprint to be 100%
 complete since it is used by scripts such as scripts/fingerwatch for
 which some partial fingerpritns are OK. */
FingerPrint *parse_single_fingerprint(char *fprint_orig) {
  int lineno = 0;
  int classno = 0; /* Number of Class lines dealt with so far */
  char *p, *q;
  char *thisline, *nextline;
  char *fprint = strdup(fprint_orig); /* Make a copy we can futz with */
  FingerPrint *FP;
  FingerPrint *current; /* Since a fingerprint is really a linked list of
			   FingerPrint structures */

  current = FP = (FingerPrint *) safe_zalloc(sizeof(FingerPrint));

  thisline = fprint;
  
  do /* 1 line at a time */ {
    nextline = strchr(thisline, '\n');
    if (nextline) *nextline++ = '\0';
    /* printf("Preparing to handle next line: %s\n", thisline); */

    while(*thisline && isspace((int) *thisline)) thisline++;
    if (!*thisline) {
      fatal("Parse error on line %d of fingerprint: %s", lineno, nextline);    
    }

    if (strncmp(thisline, "Fingerprint ", 12) == 0) {
      p = thisline + 12;
      while(*p && isspace((int) *p)) p++;

      q = strpbrk(p, "\n#");
      if (!q) q = p + strlen(p);
      while(isspace(*(--q)))
	;

      if (q < p) fatal("Parse error on line %d of fingerprint: %s", lineno, nextline);

      FP->OS_name = (char *) cp_alloc(q - p + 2);
      memcpy(FP->OS_name, p, q - p + 1);
      FP->OS_name[q - p + 1] = '\0';
      
    } else if (strncmp(thisline, "Class ", 6) == 0) {

      parse_classline(FP, thisline, lineno, &classno);

    } else if ((q = strchr(thisline, '('))) {
      *q = '\0';
      if(current->name) {
	current->next = (FingerPrint *) safe_zalloc(sizeof(FingerPrint));
	current = current->next;
      }
      current->name = strdup(thisline);
      p = q+1;
      *q = '(';
      q = strchr(p, ')');
      if (!q) {
	fatal("Parse error on line %d of fingerprint: %s\n", lineno, thisline);
      }
      *q = '\0';
      current->results = str2AVal(p);
    } else {
      fatal("Parse error line line #%d of fingerprint", lineno);
    }

    thisline = nextline; /* Time to handle the next line, if there is one */
    lineno++;
  } while (thisline && *thisline);
  return FP;
}

FingerPrint **parse_fingerprint_file(char *fname) {
FingerPrint **FPs;
FingerPrint *current;
FILE *fp;
int max_records = 4096; 
char line[512];
int numrecords = 0;
int lineno = 0;
int classno = 0; /* Number of Class lines dealt with so far */
char *p, *q; /* OH YEAH!!!! */

 FPs = (FingerPrint **) safe_zalloc(sizeof(FingerPrint *) * max_records); 

 fp = fopen(fname, "r");
 if (!fp) fatal("Unable to open Nmap fingerprint file: %s", fname);

 top:
while(fgets(line, sizeof(line), fp)) {  
  lineno++;
  /* Read in a record */
  if (*line == '\n' || *line == '#')
    continue;

 fparse:

  if (strncasecmp(line, "FingerPrint", 11)) {
    fprintf(stderr, "Parse error on line %d of nmap-os-fingerprints file: %s\n", lineno, line);
    continue;
  }

  p = line + 12;
  while(*p && isspace((int) *p)) p++;

  q = strpbrk(p, "\n#");
  if (!p) fatal("Parse error on line %d of fingerprint: %s", lineno, line);

  while(isspace(*(--q)))
    ;

  if (q < p) fatal("Parse error on line %d of fingerprint: %s", lineno, line);

  FPs[numrecords] = (FingerPrint *) safe_zalloc(sizeof(FingerPrint));

  FPs[numrecords]->OS_name = (char *) cp_alloc(q - p + 2);
  memcpy(FPs[numrecords]->OS_name, p, q - p + 1);
  FPs[numrecords]->OS_name[q - p + 1] = '\0';
      
  current = FPs[numrecords];
  current->line = lineno;
  classno = 0;

  /* Now we read the fingerprint itself */
  while(fgets(line, sizeof(line), fp)) {
    lineno++;
    if (*line == '#')
      continue;
    if (*line == '\n')
      break;
    if (!strncmp(line, "FingerPrint ",12)) {
      goto fparse;
    } else if (strncmp(line, "Class ", 6) == 0) {
      parse_classline(current, line, lineno, &classno);
    } else {
      p = line;
      q = strchr(line, '(');
      if (!q) {
	fprintf(stderr, "Parse error on line %d of nmap-os-fingerprints file: %s\n", lineno, line);
	goto top;
      }
      *q = '\0';
      if(current->name) {
	current->next = (FingerPrint *) safe_zalloc(sizeof(FingerPrint));
	current = current->next;
      }
      current->name = strdup(p);
      p = q+1;
      *q = '(';
      q = strchr(p, ')');
      if (!q) {
	fprintf(stderr, "Parse error on line %d of nmap-os-fingerprints file: %s\n", lineno, line);
	goto top;
      }
      *q = '\0';
      current->results = str2AVal(p);
    }
  }
  /* printf("Read in fingerprint:\n%s\n", fp2ascii(FPs[numrecords])); */
  numrecords++;
  if (numrecords >= max_records)
    fatal("Too many OS fingerprints -- 0verfl0w");
}
fclose(fp);
FPs[numrecords] = NULL; 
return FPs;
}

FingerPrint **parse_fingerprint_reference_file() {
char filename[256];

if (nmap_fetchfile(filename, sizeof(filename), "nmap-os-fingerprints") == -1){
  fatal("OS scan requested but I cannot find nmap-os-fingerprints file.  It should be in %s, ~/.nmap/ or .", NMAPDATADIR);
}

return parse_fingerprint_file(filename);
}

struct AVal *str2AVal(char *str) {
int i = 1;
int count = 1;
char *q = str, *p=str;
struct AVal *AVs;
if (!*str) return NULL;

/* count the AVals */
while((q = strchr(q, '%'))) {
  count++;
  q++;
}

AVs = (struct AVal *) safe_zalloc(count * sizeof(struct AVal));
for(i=0; i < count; i++) {
  q = strchr(p, '=');
  if (!q) {
    fatal("Parse error with AVal string (%s) in nmap-os-fingerprints file", str);
  }
  *q = '\0';
  AVs[i].attribute = strdup(p);
  p = q+1;
  if (i != count - 1) {
    q = strchr(p, '%');
    if (!q) {
      fatal("Parse error with AVal string (%s) in nmap-os-fingerprints file", str);
    }
    *q = '\0';
    AVs[i].next = &AVs[i+1];
  }
  Strncpy(AVs[i].value, p, sizeof(AVs[i].value)); 
  p = q + 1;
}
return AVs;
}


struct udpprobeinfo *send_closedudp_probe(int sd, const struct in_addr *victim,
					  u16 sport, u16 dport) {

static struct udpprobeinfo upi;
static int myttl = 0;
static u8 patternbyte = 0;
static u16 id = 0; 
u8 packet[328]; /* 20 IP hdr + 8 UDP hdr + 300 data */
struct ip *ip = (struct ip *) packet;
udphdr_bsd *udp = (udphdr_bsd *) (packet + sizeof(struct ip));
struct in_addr *source;
int datalen = 300;
unsigned char *data = packet + 28;
unsigned short realcheck; /* the REAL checksum */
int res;
struct sockaddr_in sock;
int decoy;
struct pseudo_udp_hdr {
  struct in_addr source;
  struct in_addr dest;        
  u8 zero;
  u8 proto;        
  u16 length;
} *pseudo = (struct pseudo_udp_hdr *) ((char *)udp - 12) ;

if (!patternbyte) patternbyte = (get_random_uint() % 60) + 65;
memset(data, patternbyte, datalen);

while(!id) id = get_random_uint();

/* check that required fields are there and not too silly */
if ( !victim || !sport || !dport || sd < 0) {
  fprintf(stderr, "send_closedudp_probe: One or more of your parameters suck!\n");
  return NULL;
}

if (!myttl)  myttl = (time(NULL) % 14) + 51;
/* It was a tough decision whether to do this here for every packet
   or let the calling function deal with it.  In the end I grudgingly decided
   to do it here and potentially waste a couple microseconds... */
sethdrinclude(sd); 

 for(decoy=0; decoy < o.numdecoys; decoy++) {
   source = &o.decoys[decoy];

   /*do we even have to fill out this damn thing?  This is a raw packet, 
     after all */
   sock.sin_family = AF_INET;
   sock.sin_port = htons(dport);
   sock.sin_addr.s_addr = victim->s_addr;


   memset((char *) packet, 0, sizeof(struct ip) + sizeof(udphdr_bsd));

   udp->uh_sport = htons(sport);
   udp->uh_dport = htons(dport);
   udp->uh_ulen = htons(8 + datalen);

   /* Now the psuedo header for checksuming */
   pseudo->source.s_addr = source->s_addr;
   pseudo->dest.s_addr = victim->s_addr;
   pseudo->proto = IPPROTO_UDP;
   pseudo->length = htons(sizeof(udphdr_bsd) + datalen);

   /* OK, now we should be able to compute a valid checksum */
realcheck = in_cksum((unsigned short *)pseudo, 20 /* pseudo + UDP headers */ +
 datalen);
#if STUPID_SOLARIS_CHECKSUM_BUG
 udp->uh_sum = sizeof(struct udphdr_bsd) + datalen;
#else
udp->uh_sum = realcheck;
#endif

   /* Goodbye, pseudo header! */
   memset(pseudo, 0, sizeof(*pseudo));

   /* Now for the ip header */
   ip->ip_v = 4;
   ip->ip_hl = 5;
   ip->ip_len = BSDFIX(sizeof(struct ip) + sizeof(udphdr_bsd) + datalen);
   ip->ip_id = id;
   ip->ip_ttl = myttl;
   ip->ip_p = IPPROTO_UDP;
   ip->ip_src.s_addr = source->s_addr;
   ip->ip_dst.s_addr= victim->s_addr;

   upi.ipck = in_cksum((unsigned short *)ip, sizeof(struct ip));
#if HAVE_IP_IP_SUM
   ip->ip_sum = upi.ipck;
#endif

   /* OK, now if this is the real she-bang (ie not a decoy) then
      we stick all the inph0 in our upi */
   if (decoy == o.decoyturn) {   
     upi.iptl = 28 + datalen;
     upi.ipid = id;
     upi.sport = sport;
     upi.dport = dport;
     upi.udpck = realcheck;
     upi.udplen = 8 + datalen;
     upi.patternbyte = patternbyte;
     upi.target.s_addr = ip->ip_dst.s_addr;
   }
   if (TCPIP_DEBUGGING > 1) {
     log_write(LOG_STDOUT, "Raw UDP packet creation completed!  Here it is:\n");
     readudppacket(packet,1);
   }
   if (TCPIP_DEBUGGING > 1)     
     log_write(LOG_STDOUT, "\nTrying sendto(%d , packet, %d, 0 , %s , %d)\n",
	    sd, BSDUFIX(ip->ip_len), inet_ntoa(*victim),
	    (int) sizeof(struct sockaddr_in));

   if ((res = sendto(sd, (const char *) packet, BSDUFIX(ip->ip_len), 0,
		     (struct sockaddr *)&sock, (int) sizeof(struct sockaddr_in))) == -1)
     {
       perror("sendto in send_udp_raw_decoys");
       return NULL;
     }

   if (TCPIP_DEBUGGING > 1) log_write(LOG_STDOUT, "successfully sent %d bytes of raw_tcp!\n", res);
 }

return &upi;

}

struct AVal *fingerprint_portunreach(struct ip *ip, struct udpprobeinfo *upi) {
struct icmp *icmp;
struct ip *ip2;
int numtests = 10;
unsigned short checksum;
unsigned short *checksumptr;
udphdr_bsd *udp;
struct AVal *AVs;
int i;
int current_testno = 0;
unsigned char *datastart, *dataend;

/* The very first thing we do is make sure this is the correct
   response */
if (ip->ip_p != IPPROTO_ICMP) {
  error("fingerprint_portunreach handed a non-ICMP packet!");
  return NULL;
}

if (ip->ip_src.s_addr != upi->target.s_addr)
  return NULL;  /* Not the person we sent to */

icmp = ((struct icmp *)  (((char *) ip) + 4 * ip->ip_hl));
if (icmp->icmp_type != 3 || icmp->icmp_code != 3)
  return NULL; /* Not a port unreachable */

ip2 = (struct ip*) ((char *)icmp + 8);
udp = (udphdr_bsd *) ((char *)ip2 + 20);

/* The ports better match as well ... */
if (ntohs(udp->uh_sport) != upi->sport || ntohs(udp->uh_dport) != upi->dport) {
  return NULL;
}

/* Create the Avals */
AVs = (struct AVal *) safe_zalloc(numtests * sizeof(struct AVal));

/* Link them together */
for(i=0; i < numtests - 1; i++)
  AVs[i].next = &AVs[i+1];

/* First of all, if we got this far the response was yes */
AVs[current_testno].attribute = "Resp";
strcpy(AVs[current_testno].value, "Y");

current_testno++;

/* Now let us do an easy one, Don't fragment */
AVs[current_testno].attribute = "DF";
  if(ntohs(ip->ip_off) & 0x4000) {
    strcpy(AVs[current_testno].value,"Y");
  } else strcpy(AVs[current_testno].value, "N");

current_testno++;

/* Now lets do TOS of the response (note, I've never seen this be
   useful */
AVs[current_testno].attribute = "TOS";
sprintf(AVs[current_testno].value, "%hX", ip->ip_tos);

current_testno++;

/* Now we look at the IP datagram length that was returned, some
   machines send more of the original packet back than others */
AVs[current_testno].attribute = "IPLEN";
sprintf(AVs[current_testno].value, "%hX", ntohs(ip->ip_len));

current_testno++;

/* OK, lets check the returned IP length, some systems @$@ this
   up */
AVs[current_testno].attribute = "RIPTL";
sprintf(AVs[current_testno].value, "%hX", ntohs(ip2->ip_len));

current_testno++;

/* This next test doesn't work on Solaris because the lamers
   overwrite our ip_id */
#if !defined(SOLARIS) && !defined(SUNOS) && !defined(IRIX)

#ifdef WIN32
if(!winip_corruption_possible()) {
#endif

/* Now lets see how they treated the ID we sent ... */
AVs[current_testno].attribute = "RID";
if (ntohs(ip2->ip_id) == 0)
  strcpy(AVs[current_testno].value, "0");
else if (ip2->ip_id == upi->ipid)
  strcpy(AVs[current_testno].value, "E"); /* The "expected" value */
else strcpy(AVs[current_testno].value, "F"); /* They fucked it up */

current_testno++;

#ifdef WIN32
}
#endif

#endif

/* Let us see if the IP checksum we got back computes */

AVs[current_testno].attribute = "RIPCK";
/* Thanks to some machines not having struct ip member ip_sum we
   have to go with this BS */
checksumptr = (unsigned short *)   ((char *) ip2 + 10);
checksum =   *checksumptr;

if (checksum == 0)
  strcpy(AVs[current_testno].value, "0");
else {
  *checksumptr = 0;
  if (in_cksum((unsigned short *)ip2, 20) == checksum) {
    strcpy(AVs[current_testno].value, "E"); /* The "expected" value */
  } else {
    strcpy(AVs[current_testno].value, "F"); /* They fucked it up */
  }
  *checksumptr = checksum;
}

current_testno++;

/* UDP checksum */
AVs[current_testno].attribute = "UCK";
if (udp->uh_sum == 0)
  strcpy(AVs[current_testno].value, "0");
else if (udp->uh_sum == upi->udpck)
  strcpy(AVs[current_testno].value, "E"); /* The "expected" value */
else strcpy(AVs[current_testno].value, "F"); /* They fucked it up */

current_testno++;

/* UDP length ... */
AVs[current_testno].attribute = "ULEN";
sprintf(AVs[current_testno].value, "%hX", ntohs(udp->uh_ulen));

current_testno++;

/* Finally we ensure the data is OK */
datastart = ((unsigned char *)udp) + 8;
dataend = (unsigned char *)  ip + ntohs(ip->ip_len);

while(datastart < dataend) {
  if (*datastart != upi->patternbyte) break;
  datastart++;
}
AVs[current_testno].attribute = "DAT";
if (datastart < dataend)
  strcpy(AVs[current_testno].value, "F"); /* They fucked it up */
else  
  strcpy(AVs[current_testno].value, "E");

AVs[current_testno].next = NULL;

return AVs;
}

/* This function takes an array of "numSamples" IP IDs and analyzes
 them to determine their sequenceability classification.  It returns
 one of the IPID_SEQ_* classifications defined in nmap.h .  If the
 function cannot determine the sequence, IPID_SEQ_UNKNOWN is returned.
 This islocalhost argument is a boolean specifying whether these
 numbers were generated by scanning localhost.  NOTE: the "ipids" argument
 may be modified if localhost is set to true. */

int ipid_sequence(int numSamples, u16 *ipids, int islocalhost) {
  u16 ipid_diffs[32];
  int i;
  int allipideqz = 1; /* Flag that means "All IP.IDs returned during
		         sequencing are zero.  This is unset if we
		         find a nonzero */
  int j,k;

  assert(numSamples < (int) (sizeof(ipid_diffs) / 2));
  if (numSamples < 2) return IPID_SEQ_UNKNOWN;

  for(i = 1; i < numSamples; i++) {

    if (ipids[i-1] != 0 || ipids[i] != 0) 
      allipideqz = 0; /* All IP.ID values do *NOT* equal zero */

    ipid_diffs[i-1] = MOD_DIFF_USHORT(ipids[i], ipids[i-1]);
    if ((ipids[i] < ipids[i-1]) && (ipids[i] > 500 || ipids[i-1] < 65000))
      return IPID_SEQ_RD;
  }

  if (allipideqz) return IPID_SEQ_ZERO;

  /* Battle plan ... 
     ipid_diffs-- if scanning localhost and safe
     If any diff is > 1000, set to random, if 0, set to constant
     If any of the diffs are 1, or all are less than 9, set to incremental 
  */
  
  if (islocalhost) {
    int allgto = 1; /* ALL diffs greater than one */
    
    for(i=0; i < numSamples - 1; i++)
      if (ipid_diffs[i] < 2) {
	allgto = 0; break;
      }
    if (allgto) {
      for(i=0; i < numSamples - 1; i++) {
	if (ipid_diffs[i] % 256 == 0) /* Stupid MS */
	  ipid_diffs[i] -= 256;
	else
	  ipid_diffs[i]--; /* Because on localhost the RST sent back ues an IPID */
      }
    }
  }

  for(i=0; i < numSamples - 1; i++) {
    if (ipid_diffs[i] > 1000) {
      return IPID_SEQ_RPI;
      break;
    }
    if (ipid_diffs[i] == 0) {
      return IPID_SEQ_CONSTANT;
      break;
    }
  }

  j = 1; /* j is a flag meaning "all differences seen are < 9" */
  k = 1; /* k is a flag meaning "all difference seen are multiples of 256 */
  for(i=0; i < numSamples - 1; i++) {
    if (ipid_diffs[i] == 1) {
      return IPID_SEQ_INCR;
    }

    if (k && ipid_diffs[i] < 2560 && ipid_diffs[i] % 256 != 0) {
      k = 0;
    }

    if (ipid_diffs[i] > 9)
      j = 0;
  }

     if (k == 1) {
       /* Stupid Microsoft! */
       return IPID_SEQ_BROKEN_INCR;
     }

     if (j == 1)
       return IPID_SEQ_INCR;

     return IPID_SEQ_UNKNOWN;

}