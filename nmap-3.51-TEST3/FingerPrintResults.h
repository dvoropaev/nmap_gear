
/***************************************************************************
 * FingerPrintResults -- The FingerPrintResults class the results of OS    *
 * fingerprint matching against a certain host.                            *
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

/* $Id: FingerPrintResults.h,v 1.5 2004/03/12 01:59:04 fyodor Exp $ */

#ifndef FINGERPRINTRESULTS_H
#define FINGERPRINTRESULTS_H

class FingerPrintResults;

#include "nmap.h"

/* Maximum number of results allowed in one of these things ... */
#define MAX_FP_RESULTS 36

struct OS_Classification_Results {
  struct OS_Classification *OSC[MAX_FP_RESULTS];
  double OSC_Accuracy[MAX_FP_RESULTS];
  int OSC_num_perfect_matches; // Number of perfect matches in OSC[\]
  int OSC_num_matches; // Number of matches total in OSC[] (and, of course, _accuracy[])
  int overall_results; /* OSSCAN_TOOMANYMATCHES, OSSCAN_NOMATCHES, OSSCAN_SUCCESS, etc */
};

class FingerPrintResults {
 public: /* For now ... a lot of the data members should be made private */
  FingerPrintResults();
  ~FingerPrintResults();

  double accuracy[MAX_FP_RESULTS]; /* Percentage of match (1.0 == perfect 
				      match) in same order as pritns[] below */
  FingerPrint *prints[MAX_FP_RESULTS]; /* ptrs to matching references -- 
					      highest accuracy matches first */
  int num_perfect_matches; /* Number of 1.0 accuracy matches in prints[] */
  int num_matches; /* Total number of matches in prints[] */
  int overall_results; /* OSSCAN_TOOMANYMATCHES, OSSCAN_NOMATCHES, 
			  OSSCAN_SUCCESS, etc */

  /* Ensures that the results are available and then returns them.  You should only call
   this AFTER all matching has been completed (because results are cached and won't change
   if new prints[] are added.)  All OS Classes in the results will be unique, and if there are 
   any perfect (accuracy 1.0) matches, only those will be returned */
  const struct OS_Classification_Results *getOSClassification();

  int osscan_opentcpport; /* Open port used for scannig (if one found -- 
			  otherwise -1) */
  int osscan_closedtcpport; /* Closed port used for scannig (if one found -- 
			    otherwise -1) */
  FingerPrint *FPs[10]; /* Fingerprint data obtained from host */
  int numFPs;
  int goodFP;

  /* Are the attributes of this fingerprint good enough to warrant submission to the official DB? */
  bool fingerprintSuitableForSubmission(); 
                                          

 private:
  bool isClassified; // Whether populateClassification() has been called
  /* Goes through fingerprinting results to populate OSR */

  void populateClassification();
  bool classAlreadyExistsInResults(struct OS_Classification *OSC);
  struct OS_Classification_Results OSR;
};

#endif /* FINGERPRINTRESULTS_H */