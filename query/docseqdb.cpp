/* Copyright (C) 2005 J.F.Dockes
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
#include <math.h>
#include <time.h>

#include <list>
using std::list;

#include "docseqdb.h"
#include "rcldb.h"
#include "debuglog.h"
#include "wasatorcl.h"

DocSequenceDb::DocSequenceDb(RefCntr<Rcl::Query> q, const string &t, 
			     RefCntr<Rcl::SearchData> sdata) 
    : DocSequence(t), m_q(q), m_sdata(sdata), m_fsdata(sdata),
      m_rescnt(-1), 
      m_queryBuildAbstract(true),
      m_queryReplaceAbstract(false),
      m_isFiltered(false),
      m_isSorted(false),
      m_needSetQuery(false),
      m_lastSQStatus(true)
{
}

void DocSequenceDb::getTerms(HighlightData& hld)
{
    m_fsdata->getTerms(hld);
}

string DocSequenceDb::getDescription() 
{
    return m_fsdata->getDescription();
}

bool DocSequenceDb::getDoc(int num, Rcl::Doc &doc, string *sh)
{
    PTMutexLocker locker(o_dblock);
    if (!setQuery())
	return false;
    if (sh) sh->erase();
    return m_q->getDoc(num, doc);
}

int DocSequenceDb::getResCnt()
{
    PTMutexLocker locker(o_dblock);
    if (!setQuery())
	return false;
    if (m_rescnt < 0) {
	m_rescnt= m_q->getResCnt();
    }
    return m_rescnt;
}

static const string cstr_mre("[...]");

// This one only gets called to fill-up the snippets window
// We ignore most abstract/snippets preferences.
bool DocSequenceDb::getAbstract(Rcl::Doc &doc, vector<Rcl::Snippet>& vpabs)
{
    LOGDEB(("DocSequenceDb::getAbstract/pair\n"));
    PTMutexLocker locker(o_dblock);
    if (!setQuery())
	return false;

    // Have to put the limit somewhere. 
    int maxoccs = 1000;
    int ret = Rcl::ABSRES_ERROR;
    if (m_q->whatDb()) {
	ret = m_q->makeDocAbstract(doc, vpabs, maxoccs, 
				   m_q->whatDb()->getAbsCtxLen()+ 2);
    } 
    LOGDEB(("DocSequenceDb::getAbstract: got ret %d vpabs len %u\n", ret,
	    vpabs.size()));
    if (vpabs.empty()) {
	return true;
    }

    // If the list was probably truncated, indicate it.
    if (ret & Rcl::ABSRES_TRUNC) {
	vpabs.push_back(Rcl::Snippet(-1, cstr_mre));
    } 
    if (ret & Rcl::ABSRES_TERMMISS) {
	vpabs.insert(vpabs.begin(), 
		     Rcl::Snippet(-1, "(Words missing in snippets)"));
    }

    return true;
}

bool DocSequenceDb::getAbstract(Rcl::Doc &doc, vector<string>& vabs)
{
    PTMutexLocker locker(o_dblock);
    if (!setQuery())
	return false;
    if (m_q->whatDb() &&
	m_queryBuildAbstract && (doc.syntabs || m_queryReplaceAbstract)) {
	m_q->makeDocAbstract(doc, vabs);
    } 
    if (vabs.empty())
	vabs.push_back(doc.meta[Rcl::Doc::keyabs]);
    return true;
}

int DocSequenceDb::getFirstMatchPage(Rcl::Doc &doc, string& term)
{
    PTMutexLocker locker(o_dblock);
    if (!setQuery())
	return false;
    if (m_q->whatDb()) {
	return m_q->getFirstMatchPage(doc, term);
    }
    return -1;
}

Rcl::Db *DocSequenceDb::getDb()
{
    return m_q.isNotNull() ? m_q->whatDb() : 0;
}

list<string> DocSequenceDb::expand(Rcl::Doc &doc)
{
    PTMutexLocker locker(o_dblock);
    if (!setQuery())
	return list<string>();
    vector<string> v = m_q->expand(doc);
    return list<string>(v.begin(), v.end());
}

string DocSequenceDb::title()
{
    string qual;
    if (m_isFiltered && !m_isSorted)
	qual = string(" (") + o_filt_trans + string(")");
    else if (!m_isFiltered && m_isSorted)
	qual = string(" (") + o_sort_trans + string(")");
    else if (m_isFiltered && m_isSorted)
	qual = string(" (") + o_sort_trans + string(",") + o_filt_trans + 
            string(")");
    return DocSequence::title() + qual;
}

bool DocSequenceDb::setFiltSpec(const DocSeqFiltSpec &fs) 
{
    LOGDEB(("DocSequenceDb::setFiltSpec\n"));
    PTMutexLocker locker(o_dblock);
    if (fs.isNotNull()) {
	// We build a search spec by adding a filtering layer to the base one.
	m_fsdata = RefCntr<Rcl::SearchData>(
	    new Rcl::SearchData(Rcl::SCLT_AND, m_sdata->getStemLang()));
	Rcl::SearchDataClauseSub *cl = 
	    new Rcl::SearchDataClauseSub(m_sdata);
	m_fsdata->addClause(cl);
    
	for (unsigned int i = 0; i < fs.crits.size(); i++) {
	    switch (fs.crits[i]) {
	    case DocSeqFiltSpec::DSFS_MIMETYPE:
		m_fsdata->addFiletype(fs.values[i]);
		break;
	    case DocSeqFiltSpec::DSFS_QLANG:
	    {
		if (m_q.isNull())
		    break;
		    
		string reason;
		Rcl::SearchData *sd = 
		    wasaStringToRcl(m_q->whatDb()->getConf(), 
				    m_sdata->getStemLang(),
				    fs.values[i], reason);
		if (sd)  {
		    Rcl::SearchDataClauseSub *cl1 = 
			new Rcl::SearchDataClauseSub(
			    RefCntr<Rcl::SearchData>(sd));
		    m_fsdata->addClause(cl1);
		}
	    }
	    break;
	    default:
		break;
	    }
	}
	m_isFiltered = true;
    } else {
	m_fsdata = m_sdata;
	m_isFiltered = false;
    }
    m_needSetQuery = true;
    return true;
}

bool DocSequenceDb::setSortSpec(const DocSeqSortSpec &spec) 
{
    LOGDEB(("DocSequenceDb::setSortSpec: fld [%s] %s\n", 
	    spec.field.c_str(), spec.desc ? "desc" : "asc"));
    PTMutexLocker locker(o_dblock);
    if (spec.isNotNull()) {
	m_q->setSortBy(spec.field, !spec.desc);
	m_isSorted = true;
    } else {
	m_q->setSortBy(string(), true);
	m_isSorted = false;
    }
    m_needSetQuery = true;
    return true;
}

bool DocSequenceDb::setQuery()
{
    if (!m_needSetQuery)
	return true;

    m_needSetQuery = false;
    m_rescnt = -1;
    m_lastSQStatus = m_q->setQuery(m_fsdata);
    if (!m_lastSQStatus) {
	m_reason = m_q->getReason();
	LOGERR(("DocSequenceDb::setQuery: rclquery::setQuery failed: %s\n",
		m_reason.c_str()));
    }
    return m_lastSQStatus;
}

bool DocSequenceDb::docDups(const Rcl::Doc& doc, std::vector<Rcl::Doc>& dups)
{
    if (m_q->whatDb()) {
        PTMutexLocker locker(o_dblock);
	return m_q->whatDb()->docDups(doc, dups);
    } else {
	return false;
    }
}
