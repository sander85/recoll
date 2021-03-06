/* Copyright (C) 2013 J.F.Dockes
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the
 *   Free Software Foundation, Inc.,
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */
#ifndef _CPUCONF_H_INCLUDED_
#define _CPUCONF_H_INCLUDED_

/** Guess how many CPUs there are on this machine, to help with configuring
    threads */
struct CpuConf {
    CpuConf()
	: ncpus(1)
    {}
    // Virtual ones, including hyperthreading, we only care about this for now
    int ncpus; 
//    int ncores;
//    int nsockets;
};

extern bool getCpuConf(CpuConf& conf);

#endif /* _CPUCONF_H_INCLUDED_ */
