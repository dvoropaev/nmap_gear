
/***************************************************************************
 * NmapOps.h -- The NmapOps class contains global options, mostly based on *
 * user-provided command-line settings.                                    *
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

/* $Id: NmapOps.h,v 1.17 2004/03/12 01:59:04 fyodor Exp $ */

class NmapOps {
 public:
  NmapOps();
  ~NmapOps();
  void ReInit(); // Reinitialize the class to default state
  void setaf(int af) { addressfamily = af; }
  int af() { return addressfamily; }
  // no setpf() because it is based on setaf() values
  int pf();
  /* Returns 0 for success, nonzero if no source has been set or any other
     failure */
  int SourceSockAddr(struct sockaddr_storage *ss, size_t *ss_len);
  /* Note that it is OK to pass in a sockaddr_in or sockaddr_in6 casted
     to sockaddr_storage */
  void setSourceSockAddr(struct sockaddr_storage *ss, size_t ss_len);

// The time this obj. was instantiated   or last ReInit()ed.
  const struct timeval *getStartTime() { return &start_time; }
  // Number of milliseconds since getStartTime().  The current time is an
  // optional argument to avoid an extre gettimeofday() call.
  int TimeSinceStartMS(struct timeval *now=NULL); 
  struct in_addr v4source();
  const struct in_addr *v4sourceip();
  bool TCPScan(); /* Returns true if at least one chosen scan type is TCP */
  bool UDPScan(); /* Returns true if at least one chosen scan type is UDP */
  bool RawScan();
  void ValidateOptions(); /* Checks that the options given are
                             reasonable and consistant.  If they aren't, the
                             function may bail out of Nmap or make small
		             adjustments (quietly or with a warning to the
		             user). */
  int isr00t;
  int debugging;
  bool packetTrace() { return (debugging >= 3)? true : pTrace;  }
  bool versionTrace() { return packetTrace()? true : vTrace;  }
  // Note that packetTrace may turn on at high debug levels even if
  // setPacketTrace(false) has been called
  void setPacketTrace(bool pt) { pTrace = pt;  }
  void setVersionTrace(bool vt) { vTrace = vt;  }
  int verbose;
  int randomize_hosts;
  int spoofsource; /* -S used */
  char device[64];
  int interactivemode;
  int host_group_sz;
  int generate_random_ips; /* -iR option */
  FingerPrint **reference_FPs;
  u16 magic_port;
  unsigned short magic_port_set; /* Was this set by user? */
  int num_ping_synprobes;
  /* The "synprobes" are also used when doing a connect() ping */
  u16 ping_synprobes[MAX_PROBE_PORTS];
  int num_ping_ackprobes;
  u16 ping_ackprobes[MAX_PROBE_PORTS];
  int num_ping_udpprobes;
  u16 ping_udpprobes[MAX_PROBE_PORTS];
  /* Scan timing/politeness issues */
  int timing_level; // 0-5, corresponding to Paranoid, Sneaky, Polite, Normal, Aggressive, Insane
  int max_parallelism; // 0 means it has not been set
  int min_parallelism; // 0 means it has not been set

  /* These functions retrieve and set the Round Trip Time timeouts, in
   milliseconds.  The set versions do extra processing to insure sane
   values and to adjust each other to insure consistance (e.g. that
   max is always at least as high as min) */
  int maxRttTimeout() { return max_rtt_timeout; }
  int minRttTimeout() { return min_rtt_timeout; }
  int initialRttTimeout() { return initial_rtt_timeout; }
  void setMaxRttTimeout(int rtt);
  void setMinRttTimeout(int rtt);
  void setInitialRttTimeout(int rtt);
  int max_ips_to_scan; // Used for Random input (-iR) to specify how 
                       // many IPs to try before stopping. 0 means unlimited.
  int extra_payload_length; /* These two are for --data_length op */
  char *extra_payload;
  unsigned long host_timeout;
  int scan_delay;
  int scanflags; /* if not -1, this value should dictate the TCP flags
		    for the core portscaning routine (eg to change a
		    FIN scan into a PSH scan.  Sort of a hack, but can
		    be very useful sometimes. */

  struct in_addr resume_ip; /* The last IP in the log file if user 
			       requested --restore .  Otherwise 
			       restore_ip.s_addr == 0.  Also 
			       target_struct_get will eventually set it 
			       to 0. */

  struct in_addr decoys[MAX_DECOYS];
  int osscan_limit; /* Skip OS Scan if no open or no closed TCP ports */
  int osscan_guess;   /* Be more aggressive in guessing OS type */
  int numdecoys;
  int decoyturn;
  int identscan;
  int osscan;
  int servicescan;
  int pingtype;
  int listscan;
  int pingscan;
  int allowall;
  int fragscan;
  int ackscan;
  int bouncescan;
  int connectscan;
  int finscan;
  int idlescan;
  int ipprotscan;
  int maimonscan;
  int nullscan;
  int rpcscan;
  int synscan;
  int udpscan;
  int windowscan;
  int xmasscan;
  int noresolve;
  int force; /* force nmap to continue on even when the outcome seems somewhat certain */
  int append_output; /* Append to any output files rather than overwrite */
  FILE *logfd[LOG_TYPES];
  FILE *nmap_stdout; /* Nmap standard output */
  int ttl; // Time to live
  char *datadir;
 private:
  int max_rtt_timeout;
  int min_rtt_timeout;
  int initial_rtt_timeout;
  void Initialize();
  int addressfamily; /*  Address family:  AF_INET or AF_INET6 */  
  struct sockaddr_storage sourcesock;
  size_t sourcesocklen;
  struct timeval start_time;
  bool pTrace; // Whether packet tracing has been enabled
  bool vTrace; // Whether version tracing has been enabled
};
  