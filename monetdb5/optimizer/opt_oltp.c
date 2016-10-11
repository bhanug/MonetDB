/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0.  If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Copyright 1997 - July 2008 CWI, August 2008 - 2016 MonetDB B.V.
 */

/* author M.Kersten
 * This optimizer prepares a MAL block for delayed locking
 * The objects are mapped to a fixed hash table to speedup testing later.
 * We don't need the actual name of the objects
 */
#include "monetdb_config.h"
#include "opt_oltp.h"

static void
addLock(OLTPlocks locks, MalBlkPtr mb, InstrPtr p, int sch, int tbl)
{	BUN hash;
	char buf[2 * IDLENGTH];

	snprintf(buf, 2 * IDLENGTH, "%s#%s", 
		(sch?getVarConstant(mb, getArg(p,sch)).val.sval : "sqlcatalog"), 
		(tbl? getVarConstant(mb, getArg(p,tbl)).val.sval : ""));
	hash = strHash(buf) % MAXOLTPLOCKS ;
	hash += (hash == 0);
	locks[hash] = 1;
}

int
OPToltpImplementation(Client cntxt, MalBlkPtr mb, MalStkPtr stk, InstrPtr pci)
{	int i, limit, slimit, updates=0;
	InstrPtr p, q, lcks;
	int actions = 0;
	InstrPtr *old;
	lng usec = GDKusec();
	OLTPlocks wlocks, rlocks;
	char buf[256];

	(void) pci;
	(void) cntxt;
	(void) stk;		/* to fool compilers */

	old= mb->stmt;
	limit= mb->stop;
	slimit = mb->ssize;
	
	// We use a fake collection of objects to speed up the checking later.
	OLTPclear(wlocks);
	OLTPclear(rlocks);

	for (i = 0; i < limit; i++) {
		p = old[i];
		if( getModuleId(p) == sqlRef && getFunctionId(p) == bindRef)
			addLock(rlocks, mb, p, p->retc + 1, p->retc + 2);
		else
		if( getModuleId(p) == sqlRef && getFunctionId(p) == bindidxRef)
			addLock(rlocks, mb, p, p->retc + 1, p->retc + 2);
		else
		if( getModuleId(p) == sqlRef && getFunctionId(p) == appendRef ){
			addLock(wlocks, mb, p, p->retc + 1, p->retc + 2);
			updates++;
		} else
		if( getModuleId(p) == sqlRef && getFunctionId(p) == updateRef ){
			addLock(wlocks, mb, p, p->retc + 1, p->retc + 2);
			updates++;
		} else
		if( getModuleId(p) == sqlRef && getFunctionId(p) == deleteRef ){
			addLock(wlocks, mb, p, p->retc + 1, p->retc + 2);
			updates++;
		} else
		if( getModuleId(p) == sqlcatalogRef ){
			addLock(wlocks, mb, p, 0,0);
			updates++;
		}
	}
	
	if( updates == 0)
		return 0;

	lcks= newInstruction(mb, ASSIGNsymbol);
	setModuleId(lcks, oltpRef);
	setFunctionId(lcks, lockRef);
	getArg(lcks,0)= newTmpVariable(mb, TYPE_void);

	for( i = 0; i< MAXOLTPLOCKS; i++)
	if( wlocks[i])
		lcks = pushInt(mb, lcks, i);
	else 
	if( rlocks[i])
		lcks = pushInt(mb, lcks, -i);

	if( lcks->argc == 1 ){
		freeInstruction(lcks);
		return 0;
	}

	// Now optimize the code
	if ( newMalBlkStmt(mb,mb->stop) < 0)
		return 0;
	pushInstruction(mb,old[0]);
	pushInstruction(mb,lcks);
	for (i = 1; i < limit; i++) {
		p = old[i];
		if( p->token == ENDsymbol){
			// unlock all if there is an error
			q= newCatchStmt(mb,"MALexception");
			q= newExitStmt(mb,"MALexception");
			q= newCatchStmt(mb,"SQLexception");
			q= newExitStmt(mb,"SQLexception");
			q= copyInstruction(lcks);
			setFunctionId(q, releaseRef);
			pushInstruction(mb,q);
		}
		pushInstruction(mb,p);
	} 
	for(; i<slimit; i++)
		if (old[i]) 
			freeInstruction(old[i]);
	GDKfree(old);

    /* Defense line against incorrect plans */
	chkTypes(cntxt->fdout, cntxt->nspace, mb, FALSE);
	//chkFlow(cntxt->fdout, mb);
	//chkDeclarations(cntxt->fdout, mb);
    /* keep all actions taken as a post block comment */
    snprintf(buf,256,"%-20s actions=%2d time=" LLFMT " usec","oltp",actions,GDKusec() - usec);
    newComment(mb,buf);
	return 1;
}
