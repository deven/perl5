
test $# -ne 0 && chdir "$1"

rm -f "t/cmd/elsif.t"
rm -f "av.c"
rm -f "tiehash"

patch -p0 <<'EOT'
*** t/op/array.t	Mon Jul 25 10:57:23 1994
--- t/op/array.t	Wed Jul 27 14:31:55 1994
***************
*** 23,31 ****
  
  if ($ary[5] eq '') {print "ok 9\n";} else {print "not ok 9\n";}
  
! $#ary += 1;	# see if we can recover element 5
  if ($#ary == 5) {print "ok 10\n";} else {print "not ok 10\n";}
! if ($ary[5] == 5) {print "ok 11\n";} else {print "not ok 11\n";}
  
  $[ = 0;
  @foo = ();
--- 23,31 ----
  
  if ($ary[5] eq '') {print "ok 9\n";} else {print "not ok 9\n";}
  
! $#ary += 1;	# see if element 5 gone for good
  if ($#ary == 5) {print "ok 10\n";} else {print "not ok 10\n";}
! if (defined $ary[5]) {print "not ok 11\n";} else {print "ok 11\n";}
  
  $[ = 0;
  @foo = ();
*** t/cmd/elsif.t	Mon May 10 14:36:19 1999
--- t/cmd/elsif.t	Mon May 10 14:36:19 1999
***************
*** 0 ****
--- 1,25 ----
+ #!./perl
+ 
+ # $RCSfile: elsif.t,v $$Revision: 4.1 $$Date: 92/08/07 18:27:08 $
+ 
+ sub foo {
+     if ($_[0] == 1) {
+ 	1;
+     }
+     elsif ($_[0] == 2) {
+ 	2;
+     }
+     elsif ($_[0] == 3) {
+ 	3;
+     }
+     else {
+ 	4;
+     }
+ }
+ 
+ print "1..4\n";
+ 
+ if (($x = &foo(1)) == 1) {print "ok 1\n";} else {print "not ok 1 '$x'\n";}
+ if (($x = &foo(2)) == 2) {print "ok 2\n";} else {print "not ok 2 '$x'\n";}
+ if (($x = &foo(3)) == 3) {print "ok 3\n";} else {print "not ok 3 '$x'\n";}
+ if (($x = &foo(4)) == 4) {print "ok 4\n";} else {print "not ok 4 '$x'\n";}
*** op.c	Mon Jul 25 13:57:40 1994
--- op.c	Tue Jul 26 11:21:08 1994
***************
*** 761,767 ****
  
      switch (op->op_type) {
      case OP_ENTERSUBR:
! 	if ((type == OP_UNDEF) && !(op->op_flags & OPf_STACKED)) {
  	    op->op_type = OP_RV2CV;		/* entersubr => rv2cv */
  	    op->op_ppaddr = ppaddr[OP_RV2CV];
  	    assert(cUNOP->op_first->op_type == OP_NULL);
--- 761,768 ----
  
      switch (op->op_type) {
      case OP_ENTERSUBR:
! 	if ((type == OP_UNDEF || type == OP_REFGEN) &&
! 	    !(op->op_flags & OPf_STACKED)) {
  	    op->op_type = OP_RV2CV;		/* entersubr => rv2cv */
  	    op->op_ppaddr = ppaddr[OP_RV2CV];
  	    assert(cUNOP->op_first->op_type == OP_NULL);
***************
*** 770,778 ****
  	}
  	/* FALL THROUGH */
      default:
- 	if (type == OP_GREPSTART)	/* grep and foreach */
- 	    break;
        nomod:
  	sprintf(tokenbuf, "Can't modify %s in %s",
  	    op_name[op->op_type],
  	    type ? op_name[type] : "local");
--- 771,780 ----
  	}
  	/* FALL THROUGH */
      default:
        nomod:
+ 	/* grep, foreach, subcalls, refgen */
+ 	if (type == OP_GREPSTART || type == OP_ENTERSUBR || type == OP_REFGEN)
+ 	    break;
  	sprintf(tokenbuf, "Can't modify %s in %s",
  	    op_name[op->op_type],
  	    type ? op_name[type] : "local");
***************
*** 876,888 ****
  	    mod(kid, type);
  	break;
      }
!     op->op_flags |= (type && type != OP_GREPSTART) ? OPf_REF|OPf_MOD : OPf_MOD;
!     if (!type) {
! 	op->op_flags &= ~OPf_SPECIAL;
  	op->op_private |= OPpLVAL_INTRO;
      }
!     else if (type == OP_AASSIGN || type == OP_SASSIGN)
! 	op->op_flags |= OPf_SPECIAL;
      return op;
  }
  
--- 878,897 ----
  	    mod(kid, type);
  	break;
      }
!     op->op_flags |= OPf_MOD;
! 
!     if (type == OP_AASSIGN || type == OP_SASSIGN)
! 	op->op_flags |= OPf_SPECIAL|OPf_REF;
!     else if (!type) {
  	op->op_private |= OPpLVAL_INTRO;
+ 	op->op_flags &= ~OPf_SPECIAL;
      }
!     else if (type == OP_REFGEN) {
! 	op->op_flags |= OPf_REF;
! 	return scalar(op);
!     }
!     else if (type != OP_GREPSTART && type != OP_ENTERSUBR)
! 	op->op_flags |= OPf_REF;
      return op;
  }
  
***************
*** 911,918 ****
  
      switch (op->op_type) {
      case OP_ENTERSUBR:
! 	if ((type == OP_REFGEN || type == OP_DEFINED)
! 	    && !(op->op_flags & (OPf_STACKED|OPf_PARENS))) {
  	    op->op_type = OP_RV2CV;             /* entersubr => rv2cv */
  	    op->op_ppaddr = ppaddr[OP_RV2CV];
  	    assert(cUNOP->op_first->op_type == OP_NULL);
--- 920,927 ----
  
      switch (op->op_type) {
      case OP_ENTERSUBR:
! 	if ((type == OP_DEFINED) &&
! 	    !(op->op_flags & OPf_STACKED)) {
  	    op->op_type = OP_RV2CV;             /* entersubr => rv2cv */
  	    op->op_ppaddr = ppaddr[OP_RV2CV];
  	    assert(cUNOP->op_first->op_type == OP_NULL);
***************
*** 954,962 ****
      case OP_AELEM:
      case OP_HELEM:
  	ref(cBINOP->op_first, op->op_type);
! 	if (type == OP_RV2AV || type == OP_RV2HV || type == OP_REFGEN) {
! 	    op->op_private |= (type == OP_RV2AV ? OPpDEREF_AV :
! 				type == OP_RV2HV ? OPpDEREF_HV : 0);
  	    op->op_flags |= OPf_MOD;
  	}
  	break;
--- 963,970 ----
      case OP_AELEM:
      case OP_HELEM:
  	ref(cBINOP->op_first, op->op_type);
! 	if (type == OP_RV2AV || type == OP_RV2HV) {
! 	    op->op_private |= (type == OP_RV2AV ? OPpDEREF_AV : OPpDEREF_HV);
  	    op->op_flags |= OPf_MOD;
  	}
  	break;
***************
*** 2376,2382 ****
      }
      else {
  	if (label->op_type == OP_ENTERSUBR)
! 	    label = newUNOP(OP_REFGEN, 0, ref(label, OP_REFGEN));
  	op = newUNOP(type, OPf_STACKED, label);
      }
      hints |= HINT_BLOCK_SCOPE;
--- 2384,2390 ----
      }
      else {
  	if (label->op_type == OP_ENTERSUBR)
! 	    label = newUNOP(OP_REFGEN, 0, mod(label, OP_REFGEN));
  	op = newUNOP(type, OPf_STACKED, label);
      }
      hints |= HINT_BLOCK_SCOPE;
***************
*** 2688,2694 ****
  OP* op;
  {
      return newUNOP(OP_REFGEN, 0,
! 	ref(list(convert(OP_ANONLIST, 0, op)), OP_REFGEN));
  }
  
  OP *
--- 2696,2702 ----
  OP* op;
  {
      return newUNOP(OP_REFGEN, 0,
! 	mod(list(convert(OP_ANONLIST, 0, op)), OP_REFGEN));
  }
  
  OP *
***************
*** 2696,2702 ****
  OP* op;
  {
      return newUNOP(OP_REFGEN, 0,
! 	ref(list(convert(OP_ANONHASH, 0, op)), OP_REFGEN));
  }
  
  OP *
--- 2704,2710 ----
  OP* op;
  {
      return newUNOP(OP_REFGEN, 0,
! 	mod(list(convert(OP_ANONHASH, 0, op)), OP_REFGEN));
  }
  
  OP *
***************
*** 3444,3449 ****
--- 3452,3459 ----
      op->op_private = (hints & HINT_STRICT_REFS);
      if (perldb && curstash != debstash)
  	op->op_private |= OPpDEREF_DB;
+     while (o = o->op_sibling)
+ 	mod(o, OP_ENTERSUBR);
      return op;
  }
  
*** av.c	Mon May 10 14:36:19 1999
--- av.c	Mon May 10 14:36:19 1999
***************
*** 0 ****
--- 1,397 ----
+ /* $RCSfile: array.c,v $$Revision: 4.1 $$Date: 92/08/07 17:18:22 $
+  *
+  *    Copyright (c) 1991-1994, Larry Wall
+  *
+  *    You may distribute under the terms of either the GNU General Public
+  *    License or the Artistic License, as specified in the README file.
+  *
+  * $Log:	array.c,v $
+  */
+ 
+ /*
+  * "...for the Entwives desired order, and plenty, and peace (by which they
+  * meant that things should remain where they had set them)." --Treebeard
+  */
+ 
+ #include "EXTERN.h"
+ #include "perl.h"
+ 
+ static void
+ av_reify(av)
+ AV* av;
+ {
+     I32 key;
+     SV* sv;
+     
+     key = AvFILL(av) + 1;
+     while (key) {
+ 	sv = AvARRAY(av)[--key];
+ 	assert(sv);
+ 	if (sv != &sv_undef)
+ 	    SvREFCNT_inc(sv);
+     }
+     AvREAL_on(av);
+ }
+ 
+ SV**
+ av_fetch(av,key,lval)
+ register AV *av;
+ I32 key;
+ I32 lval;
+ {
+     SV *sv;
+ 
+     if (SvRMAGICAL(av)) {
+ 	if (mg_find((SV*)av,'P')) {
+ 	    sv = sv_newmortal();
+ 	    mg_copy((SV*)av, sv, 0, key);
+ 	    Sv = sv;
+ 	    return &Sv;
+ 	}
+     }
+ 
+     if (key < 0) {
+ 	key += AvFILL(av) + 1;
+ 	if (key < 0)
+ 	    return 0;
+     }
+     else if (key > AvFILL(av)) {
+ 	if (!lval)
+ 	    return 0;
+ 	if (AvREALISH(av))
+ 	    sv = NEWSV(5,0);
+ 	else
+ 	    sv = sv_newmortal();
+ 	return av_store(av,key,sv);
+     }
+     if (AvARRAY(av)[key] == &sv_undef) {
+ 	if (lval) {
+ 	    sv = NEWSV(6,0);
+ 	    return av_store(av,key,sv);
+ 	}
+ 	return 0;
+     }
+     return &AvARRAY(av)[key];
+ }
+ 
+ SV**
+ av_store(av,key,val)
+ register AV *av;
+ I32 key;
+ SV *val;
+ {
+     I32 tmp;
+     SV** ary;
+ 
+     if (SvRMAGICAL(av)) {
+ 	if (mg_find((SV*)av,'P')) {
+ 	    mg_copy((SV*)av, val, 0, key);
+ 	    return 0;
+ 	}
+     }
+ 
+     if (key < 0) {
+ 	key += AvFILL(av) + 1;
+ 	if (key < 0)
+ 	    return 0;
+     }
+     if (!val)
+ 	val = &sv_undef;
+ 
+     if (key > AvMAX(av)) {
+ 	I32 newmax;
+ 
+ 	if (AvALLOC(av) != AvARRAY(av)) {
+ 	    ary = AvALLOC(av) + AvFILL(av) + 1;
+ 	    tmp = AvARRAY(av) - AvALLOC(av);
+ 	    Move(AvARRAY(av), AvALLOC(av), AvMAX(av)+1, SV*);
+ 	    AvMAX(av) += tmp;
+ 	    SvPVX(av) = (char*)(AvALLOC(av));
+ 	    while (tmp)
+ 		ary[--tmp] = &sv_undef;
+ 	    
+ 	    if (key > AvMAX(av) - 10) {
+ 		newmax = key + AvMAX(av);
+ 		goto resize;
+ 	    }
+ 	}
+ 	else {
+ 	    if (AvALLOC(av)) {
+ 		newmax = key + AvMAX(av) / 5;
+ 	      resize:
+ 		Renew(AvALLOC(av),newmax+1, SV*);
+ 		ary = AvALLOC(av) + AvMAX(av) + 1;
+ 		tmp = newmax - AvMAX(av);
+ 	    }
+ 	    else {
+ 		newmax = key < 4 ? 4 : key;
+ 		New(2,AvALLOC(av), newmax+1, SV*);
+ 		ary = AvALLOC(av);
+ 		tmp = newmax + 1;
+ 	    }
+ 	    while (tmp)
+ 		ary[--tmp] = &sv_undef;
+ 	    
+ 	    SvPVX(av) = (char*)AvALLOC(av);
+ 	    AvMAX(av) = newmax;
+ 	}
+     }
+     if (AvREIFY(av))
+ 	av_reify(av);
+     if (AvFILL(av) < key)
+ 	AvFILL(av) = key;
+ 
+     ary = AvARRAY(av);
+     ary[key] = val;
+     if (SvSMAGICAL(av)) {
+ 	if (val != &sv_undef) {
+ 	    MAGIC* mg = SvMAGIC(av);
+ 	    sv_magic(val, (SV*)av, toLOWER(mg->mg_type), 0, key);
+ 	}
+ 	mg_set((SV*)av);
+     }
+     return &ary[key];
+ }
+ 
+ AV *
+ newAV()
+ {
+     register AV *av;
+ 
+     Newz(1,av,1,AV);
+     SvREFCNT(av) = 1;
+     sv_upgrade((SV *)av,SVt_PVAV);
+     AvREAL_on(av);
+     AvALLOC(av) = 0;
+     SvPVX(av) = 0;
+     AvMAX(av) = AvFILL(av) = -1;
+     return av;
+ }
+ 
+ AV *
+ av_make(size,strp)
+ register I32 size;
+ register SV **strp;
+ {
+     register AV *av;
+     register I32 i;
+     register SV** ary;
+ 
+     av = (AV*)newSV(0);
+     sv_upgrade((SV *) av,SVt_PVAV);
+     New(4,ary,size+1,SV*);
+     AvALLOC(av) = ary;
+     AvFLAGS(av) = AVf_REAL;
+     SvPVX(av) = (char*)ary;
+     AvFILL(av) = size - 1;
+     AvMAX(av) = size - 1;
+     for (i = 0; i < size; i++) {
+ 	assert (*strp);
+ 	ary[i] = NEWSV(7,0);
+ 	sv_setsv(ary[i], *strp);
+ 	strp++;
+     }
+     return av;
+ }
+ 
+ AV *
+ av_fake(size,strp)
+ register I32 size;
+ register SV **strp;
+ {
+     register AV *av;
+     register SV** ary;
+ 
+     Newz(3,av,1,AV);
+     SvREFCNT(av) = 1;
+     sv_upgrade((SV *) av,SVt_PVAV);
+     New(4,ary,size+1,SV*);
+     AvALLOC(av) = ary;
+     Copy(strp,ary,size,SV*);
+     AvFLAGS(av) = AVf_REIFY;
+     SvPVX(av) = (char*)ary;
+     AvFILL(av) = size - 1;
+     AvMAX(av) = size - 1;
+     while (size--) {
+ 	assert (*strp);
+ 	SvTEMP_off(*strp);
+ 	strp++;
+     }
+     return av;
+ }
+ 
+ void
+ av_clear(av)
+ register AV *av;
+ {
+     register I32 key;
+     SV** ary;
+ 
+     if (!av || AvMAX(av) < 0)
+ 	return;
+     /*SUPPRESS 560*/
+ 
+     ary = AvARRAY(av);
+     key = AvFILL(av) + 1;
+     if (AvREAL(av)) {
+ 	while (key) {
+ 	    SvREFCNT_dec(ary[--key]);
+ 	    ary[key] = &sv_undef;
+ 	}
+     }
+     else {
+ 	while (key)
+ 	    ary[--key] = &sv_undef;
+     }
+     if (key = AvARRAY(av) - AvALLOC(av)) {
+ 	AvMAX(av) += key;
+ 	SvPVX(av) = (char*)(AvARRAY(av) - key);
+     }
+     AvFILL(av) = -1;
+ }
+ 
+ void
+ av_undef(av)
+ register AV *av;
+ {
+     register I32 key;
+ 
+     if (!av)
+ 	return;
+     /*SUPPRESS 560*/
+     if (AvREAL(av)) {
+ 	key = AvFILL(av) + 1;
+ 	while (key)
+ 	    SvREFCNT_dec(AvARRAY(av)[--key]);
+     }
+     if (key = AvARRAY(av) - AvALLOC(av)) {
+ 	AvMAX(av) += key;
+ 	SvPVX(av) = (char*)(AvARRAY(av) - key);
+     }
+     Safefree(AvALLOC(av));
+     AvALLOC(av) = 0;
+     SvPVX(av) = 0;
+     AvMAX(av) = AvFILL(av) = -1;
+ }
+ 
+ bool
+ av_push(av,val)
+ register AV *av;
+ SV *val;
+ {
+     return av_store(av,++(AvFILL(av)),val) != 0;
+ }
+ 
+ SV *
+ av_pop(av)
+ register AV *av;
+ {
+     SV *retval;
+ 
+     if (AvFILL(av) < 0)
+ 	return &sv_undef;
+     retval = AvARRAY(av)[AvFILL(av)];
+     AvARRAY(av)[AvFILL(av)--] = &sv_undef;
+     if (SvSMAGICAL(av))
+ 	mg_set((SV*)av);
+     return retval;
+ }
+ 
+ void
+ av_unshift(av,num)
+ register AV *av;
+ register I32 num;
+ {
+     register I32 i;
+     register SV **sstr,**dstr;
+ 
+     if (num <= 0)
+ 	return;
+     if (!AvREAL(av)) {
+ 	if (AvREIFY(av))
+ 	    av_reify(av);
+ 	else
+ 	    croak("Can't unshift");
+     }
+     i = AvARRAY(av) - AvALLOC(av);
+     if (i > num)
+ 	i = num;
+     num -= i;
+     
+     AvMAX(av) += i;
+     AvFILL(av) += i;
+     SvPVX(av) = (char*)(AvARRAY(av) - i);
+ 
+     if (num) {
+ 	(void)av_store(av,AvFILL(av)+num,(SV*)0);	/* maybe extend array */
+ 	dstr = AvARRAY(av) + AvFILL(av);
+ 	sstr = dstr - num;
+ #ifdef BUGGY_MSC5
+  # pragma loop_opt(off)	/* don't loop-optimize the following code */
+ #endif /* BUGGY_MSC5 */
+ 	for (i = AvFILL(av) - num; i >= 0; --i) {
+ 	    *dstr-- = *sstr--;
+ #ifdef BUGGY_MSC5
+  # pragma loop_opt()	/* loop-optimization back to command-line setting */
+ #endif /* BUGGY_MSC5 */
+ 	}
+ 	while (num)
+ 	    AvARRAY(av)[--num] = &sv_undef;
+     }
+ }
+ 
+ SV *
+ av_shift(av)
+ register AV *av;
+ {
+     SV *retval;
+ 
+     if (AvFILL(av) < 0)
+ 	return &sv_undef;
+     retval = *AvARRAY(av);
+     *AvARRAY(av) = &sv_undef;
+     SvPVX(av) = (char*)(AvARRAY(av) + 1);
+     AvMAX(av)--;
+     AvFILL(av)--;
+     if (SvSMAGICAL(av))
+ 	mg_set((SV*)av);
+     return retval;
+ }
+ 
+ I32
+ av_len(av)
+ register AV *av;
+ {
+     return AvFILL(av);
+ }
+ 
+ void
+ av_fill(av, fill)
+ register AV *av;
+ I32 fill;
+ {
+     if (fill < 0)
+ 	fill = -1;
+     if (fill <= AvMAX(av)) {
+ 	if (AvFILL(av) > fill) {
+ 	    I32 key = AvFILL(av);
+ 	    SV** ary = AvARRAY(av);
+ 
+ 	    if (AvREAL(av)) {
+ 		while (key > fill) {
+ 		    SvREFCNT_dec(ary[key]);
+ 		    ary[key--] = &sv_undef;
+ 		}
+ 	    }
+ 	    else {
+ 		while (key > fill)
+ 		    ary[key--] = &sv_undef;
+ 	    }
+ 	}
+ 	AvFILL(av) = fill;
+ 	if (SvSMAGICAL(av))
+ 	    mg_set((SV*)av);
+     }
+     else
+ 	(void)av_store(av,fill,&sv_undef);
+ }
*** av.h	Mon Jul 25 10:58:34 1994
--- av.h	Wed Jul 27 13:57:05 1994
***************
*** 34,42 ****
  #define AvARYLEN(av)	((XPVAV*)  SvANY(av))->xav_arylen
  #define AvFLAGS(av)	((XPVAV*)  SvANY(av))->xav_flags
  
! #define AvREAL(av)	(((XPVAV*)  SvANY(av))->xav_flags & AVf_REAL)
! #define AvREAL_on(av)	(((XPVAV*)  SvANY(av))->xav_flags |= AVf_REAL)
! #define AvREAL_off(av)	(((XPVAV*)  SvANY(av))->xav_flags &= ~AVf_REAL)
! #define AvREIFY(av)	(((XPVAV*)  SvANY(av))->xav_flags & AVf_REIFY)
! #define AvREIFY_on(av)	(((XPVAV*)  SvANY(av))->xav_flags |= AVf_REIFY)
! #define AvREIFY_off(av)	(((XPVAV*)  SvANY(av))->xav_flags &= ~AVf_REIFY)
--- 34,45 ----
  #define AvARYLEN(av)	((XPVAV*)  SvANY(av))->xav_arylen
  #define AvFLAGS(av)	((XPVAV*)  SvANY(av))->xav_flags
  
! #define AvREAL(av)	(AvFLAGS(av) & AVf_REAL)
! #define AvREAL_on(av)	(AvFLAGS(av) |= AVf_REAL)
! #define AvREAL_off(av)	(AvFLAGS(av) &= ~AVf_REAL)
! #define AvREIFY(av)	(AvFLAGS(av) & AVf_REIFY)
! #define AvREIFY_on(av)	(AvFLAGS(av) |= AVf_REIFY)
! #define AvREIFY_off(av)	(AvFLAGS(av) &= ~AVf_REIFY)
! 
! #define AvREALISH(av)	AvFLAGS(av)	/* REAL or REIFY -- shortcut */
! 
*** pp.c	Mon Jul 25 13:57:42 1994
--- pp.c	Wed Jul 27 15:38:58 1994
***************
*** 239,245 ****
      if (op->op_flags & OPf_REF)
  	RETURN;
      if (GIMME == G_ARRAY) { /* array wanted */
! 	return do_kv(ARGS);
      }
      else {
  	SV* sv = sv_newmortal();
--- 239,245 ----
      if (op->op_flags & OPf_REF)
  	RETURN;
      if (GIMME == G_ARRAY) { /* array wanted */
! 	RETURNOP(do_kv(ARGS));
      }
      else {
  	SV* sv = sv_newmortal();
***************
*** 1289,1296 ****
  	case SVt_PVAV:
  	    ary = (AV*)sv;
  	    magic = SvSMAGICAL(ary) != 0;
! 	    AvREAL_on(ary);
! 	    AvFILL(ary) = -1;
  	    i = 0;
  	    while (relem <= lastrelem) {	/* gobble up all the rest */
  		sv = NEWSV(28,0);
--- 1289,1296 ----
  	case SVt_PVAV:
  	    ary = (AV*)sv;
  	    magic = SvSMAGICAL(ary) != 0;
! 	    
! 	    av_fill(ary, -1);
  	    i = 0;
  	    while (relem <= lastrelem) {	/* gobble up all the rest */
  		sv = NEWSV(28,0);
***************
*** 4860,4866 ****
  		for (i = offset; i > 0; i--)	/* can't trust Copy */
  		    *dst-- = *src--;
  	    }
! 	    Zero(AvARRAY(ary), -diff, SV*);
  	    SvPVX(ary) = (char*)(AvARRAY(ary) - diff); /* diff is negative */
  	    AvMAX(ary) += diff;
  	}
--- 4860,4866 ----
  		for (i = offset; i > 0; i--)	/* can't trust Copy */
  		    *dst-- = *src--;
  	    }
! 	    dst = AvARRAY(ary);
  	    SvPVX(ary) = (char*)(AvARRAY(ary) - diff); /* diff is negative */
  	    AvMAX(ary) += diff;
  	}
***************
*** 4870,4878 ****
  		dst = src + diff;		/* diff is negative */
  		Move(src, dst, after, SV*);
  	    }
! 	    Zero(&AvARRAY(ary)[AvFILL(ary)+1], -diff, SV*);
  						/* avoid later double free */
  	}
  	if (newlen) {
  	    for (src = tmparyval, dst = AvARRAY(ary) + offset;
  	      newlen; newlen--) {
--- 4870,4882 ----
  		dst = src + diff;		/* diff is negative */
  		Move(src, dst, after, SV*);
  	    }
! 	    dst = &AvARRAY(ary)[AvFILL(ary)+1];
  						/* avoid later double free */
  	}
+ 	i = -diff;
+ 	while (i)
+ 	    dst[--i] = &sv_undef;
+ 	
  	if (newlen) {
  	    for (src = tmparyval, dst = AvARRAY(ary) + offset;
  	      newlen; newlen--) {
***************
*** 4907,4918 ****
  		    av_store(ary, AvFILL(ary) + diff, Nullsv);
  		else
  		    AvFILL(ary) += diff;
! 		dst = AvARRAY(ary) + AvFILL(ary);
! 		for (i = diff; i > 0; i--) {
! 		    if (*dst)			/* stuff was hanging around */
! 			SvREFCNT_dec(*dst);		/*  after $#foo */
! 		    dst--;
! 		}
  		if (after) {
  		    dst = AvARRAY(ary) + AvFILL(ary);
  		    src = dst - diff;
--- 4911,4917 ----
  		    av_store(ary, AvFILL(ary) + diff, Nullsv);
  		else
  		    AvFILL(ary) += diff;
! 
  		if (after) {
  		    dst = AvARRAY(ary) + AvFILL(ary);
  		    src = dst - diff;
***************
*** 5790,5804 ****
  	    GvAV(defgv) = cx->blk_sub.argarray;
  	    ++MARK;
  
! 	    if (items >= AvMAX(av)) {
  		ary = AvALLOC(av);
  		if (AvARRAY(av) != ary) {
  		    AvMAX(av) += AvARRAY(av) - AvALLOC(av);
  		    SvPVX(av) = (char*)ary;
  		}
! 		if (items >= AvMAX(av)) {
  		    AvMAX(av) = items - 1;
! 		    Renew(ary,items+1,SV*);
  		    AvALLOC(av) = ary;
  		    SvPVX(av) = (char*)ary;
  		}
--- 5789,5807 ----
  	    GvAV(defgv) = cx->blk_sub.argarray;
  	    ++MARK;
  
! 	    if (AvREAL(av)) {
! 		av_clear(av);
! 		AvREAL_off(av);
! 	    }
! 	    if (items > AvMAX(av) + 1) {
  		ary = AvALLOC(av);
  		if (AvARRAY(av) != ary) {
  		    AvMAX(av) += AvARRAY(av) - AvALLOC(av);
  		    SvPVX(av) = (char*)ary;
  		}
! 		if (items > AvMAX(av) + 1) {
  		    AvMAX(av) = items - 1;
! 		    Renew(ary,items,SV*);
  		    AvALLOC(av) = ary;
  		    SvPVX(av) = (char*)ary;
  		}
***************
*** 5805,5810 ****
--- 5808,5814 ----
  	    }
  	    Copy(MARK,AvARRAY(av),items,SV*);
  	    AvFILL(av) = items - 1;
+ 	    
  	    while (items--) {
  		if (*MARK)
  		    SvTEMP_off(*MARK);
***************
*** 5844,5849 ****
--- 5848,5860 ----
  	    if (!(SvFLAGS(*mark) & (SVs_PADTMP|SVs_TEMP)))
  		*mark = sv_mortalcopy(*mark);
  		/* in case LEAVE wipes old return values */
+     }
+ 
+     if (cx->blk_sub.hasargs) {		/* You don't exist; go away. */
+ 	AV* av = cx->blk_sub.argarray;
+ 
+ 	av_clear(av);
+ 	AvREAL_off(av);
      }
  
      LEAVE;
*** perly.c	Mon Jul 25 10:59:26 1994
--- perly.c	Tue Jul 26 10:40:41 1994
***************
*** 2274,2280 ****
  break;
  case 92:
  #line 389 "perly.y"
! { yyval.opval = newUNOP(OP_REFGEN, 0, ref(yyvsp[0].opval,OP_REFGEN)); }
  break;
  case 93:
  #line 391 "perly.y"
--- 2274,2280 ----
  break;
  case 92:
  #line 389 "perly.y"
! { yyval.opval = newUNOP(OP_REFGEN, 0, mod(yyvsp[0].opval,OP_REFGEN)); }
  break;
  case 93:
  #line 391 "perly.y"
*** perly.y	Mon Jul 25 10:59:19 1994
--- perly.y	Tue Jul 26 10:11:03 1994
***************
*** 386,392 ****
  	|	'~' term
  			{ $$ = newUNOP(OP_COMPLEMENT, 0, scalar($2));}
  	|	REFGEN term
! 			{ $$ = newUNOP(OP_REFGEN, 0, ref($2,OP_REFGEN)); }
  	|	term POSTINC
  			{ $$ = newUNOP(OP_POSTINC, 0,
  					mod(scalar($1), OP_POSTINC)); }
--- 386,392 ----
  	|	'~' term
  			{ $$ = newUNOP(OP_COMPLEMENT, 0, scalar($2));}
  	|	REFGEN term
! 			{ $$ = newUNOP(OP_REFGEN, 0, mod($2,OP_REFGEN)); }
  	|	term POSTINC
  			{ $$ = newUNOP(OP_POSTINC, 0,
  					mod(scalar($1), OP_POSTINC)); }
*** doop.c	Wed Jul 27 15:28:34 1994
--- doop.c	Tue Jul 26 15:13:33 1994
***************
*** 544,551 ****
      register HE *entry;
      char *tmps;
      SV *tmpstr;
!     I32 dokeys =   (op->op_type == OP_KEYS   || op->op_type == OP_RV2HV);
!     I32 dovalues = (op->op_type == OP_VALUES || op->op_type == OP_RV2HV);
  
      if (!hv)
  	RETURN;
--- 544,554 ----
      register HE *entry;
      char *tmps;
      SV *tmpstr;
!     I32 dokeys =   (op->op_type == OP_KEYS);
!     I32 dovalues = (op->op_type == OP_VALUES);
! 
!     if (op->op_type == OP_RV2HV || op->op_type == OP_PADHV) 
! 	dokeys = dovalues = TRUE;
  
      if (!hv)
  	RETURN;
*** tiehash	Mon May 10 14:36:17 1999
--- tiehash	Mon May 10 14:36:17 1999
***************
*** 0 ****
--- 1,30 ----
+ #!./perl
+ 
+ # $RCSfile: dbm.t,v $$Revision: 4.1 $$Date: 92/08/07 18:27:43 $
+ 
+ {
+     package XDBM_File;
+     sub new { print "new @_\n"; bless {FOO => 'foo'} }
+     sub FETCH { print "FETCH @_\n"; $_[0]->{$_[1]} }
+     sub STORE { print "STORE @_\n"; $_[0]->{$_[1]} = $_[2] }
+     sub DELETE { print "DELETE @_\n"; delete ${$_[0]}{$_[1]} }
+     sub FIRSTKEY { print "FIRSTKEY @_\n"; each %{$_[0]} }
+     sub NEXTKEY { print "NEXTKEY @_\n"; each %{$_[0]} }
+     sub DESTROY { print "DESTROY @_\n"; undef %{$_[0]}; }
+ }
+ 
+ tie %h, XDBM_File;
+ 
+ $h{BAR} = 'bar';
+ $h{FOO} = 'foo';
+ #print $h{BAR}, "\n";
+ #delete $h{BAR};
+ #print $h{BAR}, "\n";
+ 
+ while (($key,$val) = each %h) { print "$key => $val\n"; }
+ @keys = sort keys %h;
+ @values = sort values %h;
+ print "@keys\n@values\n";
+ 
+ untie %h;
+ 
*** mg.c	Mon Jul 25 10:59:14 1994
--- mg.c	Wed Jul 27 13:51:18 1994
***************
*** 785,791 ****
  SV* sv;
  MAGIC* mg;
  {
!     av_fill((AV*)mg->mg_obj, (SvIOK(sv) ? SvIVX(sv) : sv_2iv(sv)) - arybase);
      return 0;
  }
  
--- 785,791 ----
  SV* sv;
  MAGIC* mg;
  {
!     av_fill((AV*)mg->mg_obj, SvIV(sv) - arybase);
      return 0;
  }
  
*** sv.c	Mon Jul 25 10:58:37 1994
--- sv.c	Tue Jul 26 12:09:47 1994
***************
*** 689,779 ****
  register SV *sv;
  {
      char *t = tokenbuf;
!     *t = '\0';
  
    retry:
      if (!sv) {
  	strcpy(t, "VOID");
! 	return tokenbuf;
      }
      else if (sv == (SV*)0x55555555 || SvTYPE(sv) == 'U') {
  	strcpy(t, "WILD");
! 	return tokenbuf;
      }
!     else if (SvREFCNT(sv) == 0 && !SvREADONLY(sv)) {
! 	strcpy(t, "UNREF");
! 	return tokenbuf;
      }
!     else {
! 	switch (SvTYPE(sv)) {
! 	default:
! 	    strcpy(t,"FREED");
! 	    return tokenbuf;
! 	    break;
  
! 	case SVt_NULL:
! 	    strcpy(t,"UNDEF");
! 	    return tokenbuf;
! 	case SVt_IV:
! 	    strcpy(t,"IV");
! 	    break;
! 	case SVt_NV:
! 	    strcpy(t,"NV");
! 	    break;
! 	case SVt_RV:
! 	    *t++ = '\\';
! 	    if (t - tokenbuf > 10) {
! 		strcpy(tokenbuf + 3,"...");
! 		return tokenbuf;
! 	    }
! 	    sv = (SV*)SvRV(sv);
! 	    goto retry;
! 	case SVt_PV:
! 	    strcpy(t,"PV");
! 	    break;
! 	case SVt_PVIV:
! 	    strcpy(t,"PVIV");
! 	    break;
! 	case SVt_PVNV:
! 	    strcpy(t,"PVNV");
! 	    break;
! 	case SVt_PVMG:
! 	    strcpy(t,"PVMG");
! 	    break;
! 	case SVt_PVLV:
! 	    strcpy(t,"PVLV");
! 	    break;
! 	case SVt_PVAV:
! 	    strcpy(t,"AV");
! 	    break;
! 	case SVt_PVHV:
! 	    strcpy(t,"HV");
! 	    break;
! 	case SVt_PVCV:
! 	    if (CvGV(sv))
! 		sprintf(t, "CV(%s)", GvNAME(CvGV(sv)));
! 	    else
! 		strcpy(t, "CV()");
! 	    return tokenbuf;
! 	case SVt_PVGV:
! 	    strcpy(t,"GV");
! 	    break;
! 	case SVt_PVBM:
! 	    strcpy(t,"BM");
! 	    break;
! 	case SVt_PVFM:
! 	    strcpy(t,"FM");
! 	    break;
! 	case SVt_PVIO:
! 	    strcpy(t,"IO");
! 	    break;
  	}
      }
      t += strlen(t);
  
      if (SvPOK(sv)) {
  	if (!SvPVX(sv))
! 	    return "(null)";
  	if (SvOOK(sv))
  	    sprintf(t,"(%ld+\"%.127s\")",(long)SvIVX(sv),SvPVX(sv));
  	else
--- 689,777 ----
  register SV *sv;
  {
      char *t = tokenbuf;
!     int unref = 0;
  
    retry:
      if (!sv) {
  	strcpy(t, "VOID");
! 	goto finish;
      }
      else if (sv == (SV*)0x55555555 || SvTYPE(sv) == 'U') {
  	strcpy(t, "WILD");
! 	goto finish;
      }
!     else if (SvREFCNT(sv) == 0 &&
! 	     sv != &sv_undef && sv != &sv_no && sv != &sv_yes) {
! 	*t++ = '(';
! 	unref++;
      }
!     switch (SvTYPE(sv)) {
!     default:
! 	strcpy(t,"FREED");
! 	goto finish;
  
!     case SVt_NULL:
! 	strcpy(t,"UNDEF");
! 	return tokenbuf;
!     case SVt_IV:
! 	strcpy(t,"IV");
! 	break;
!     case SVt_NV:
! 	strcpy(t,"NV");
! 	break;
!     case SVt_RV:
! 	*t++ = '\\';
! 	if (t - tokenbuf + unref > 10) {
! 	    strcpy(tokenbuf + unref + 3,"...");
! 	    goto finish;
  	}
+ 	sv = (SV*)SvRV(sv);
+ 	goto retry;
+     case SVt_PV:
+ 	strcpy(t,"PV");
+ 	break;
+     case SVt_PVIV:
+ 	strcpy(t,"PVIV");
+ 	break;
+     case SVt_PVNV:
+ 	strcpy(t,"PVNV");
+ 	break;
+     case SVt_PVMG:
+ 	strcpy(t,"PVMG");
+ 	break;
+     case SVt_PVLV:
+ 	strcpy(t,"PVLV");
+ 	break;
+     case SVt_PVAV:
+ 	strcpy(t,"AV");
+ 	break;
+     case SVt_PVHV:
+ 	strcpy(t,"HV");
+ 	break;
+     case SVt_PVCV:
+ 	if (CvGV(sv))
+ 	    sprintf(t, "CV(%s)", GvNAME(CvGV(sv)));
+ 	else
+ 	    strcpy(t, "CV()");
+ 	goto finish;
+     case SVt_PVGV:
+ 	strcpy(t,"GV");
+ 	break;
+     case SVt_PVBM:
+ 	strcpy(t,"BM");
+ 	break;
+     case SVt_PVFM:
+ 	strcpy(t,"FM");
+ 	break;
+     case SVt_PVIO:
+ 	strcpy(t,"IO");
+ 	break;
      }
      t += strlen(t);
  
      if (SvPOK(sv)) {
  	if (!SvPVX(sv))
! 	    strcpy(t, "(null)");
  	if (SvOOK(sv))
  	    sprintf(t,"(%ld+\"%.127s\")",(long)SvIVX(sv),SvPVX(sv));
  	else
***************
*** 785,790 ****
--- 783,796 ----
  	sprintf(t,"(%ld)",(long)SvIVX(sv));
      else
  	strcpy(t,"()");
+     
+   finish:
+     if (unref) {
+ 	t += strlen(t);
+ 	while (unref--)
+ 	    *t++ = ')';
+ 	*t = '\0';
+     }
      return tokenbuf;
  }
  
*** deb.c	Mon Jul 25 10:58:01 1994
--- deb.c	Tue Jul 26 14:37:01 1994
***************
*** 100,106 ****
  	    break;
  
      fprintf(stderr, i ? "    =>  ...  " : "    =>  ");
!     if (stack_base[0] || stack_sp < stack_base)
  	fprintf(stderr, " [STACK UNDERFLOW!!!]\n");
      do {
  	i++;
--- 100,106 ----
  	    break;
  
      fprintf(stderr, i ? "    =>  ...  " : "    =>  ");
!     if (stack_base[0] != &sv_undef || stack_sp < stack_base)
  	fprintf(stderr, " [STACK UNDERFLOW!!!]\n");
      do {
  	i++;
*** dump.c	Mon Jul 25 10:59:22 1994
--- dump.c	Tue Jul 26 14:45:37 1994
***************
*** 207,213 ****
  	    if (op->op_private & OPpFLIP_LINENUM)
  		(void)strcat(buf,"LINENUM,");
  	}
! 	if (op->op_flags & OPf_REF && op->op_private & OPpLVAL_INTRO)
  	    (void)strcat(buf,"INTRO,");
  	if (*buf) {
  	    buf[strlen(buf)-1] = '\0';
--- 207,213 ----
  	    if (op->op_private & OPpFLIP_LINENUM)
  		(void)strcat(buf,"LINENUM,");
  	}
! 	if (op->op_flags & OPf_MOD && op->op_private & OPpLVAL_INTRO)
  	    (void)strcat(buf,"INTRO,");
  	if (*buf) {
  	    buf[strlen(buf)-1] = '\0';
EOT

exit 0
