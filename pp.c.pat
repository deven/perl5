***************
*** 1810,2000 ****
  
  PP(pp_left_shift)
  {
!     dSP; dATARGET; dPOPTOPiirl;
!     SETi( left << right );
!     RETURN;
  }
  
  PP(pp_right_shift)
  {
!     dSP; dATARGET; dPOPTOPiirl;
!     SETi( left >> right );
!     RETURN;
  }
  
  PP(pp_lt)
  {
!     dSP; dPOPnv;
!     SETs((TOPn < value) ? &sv_yes : &sv_no);
!     RETURN;
  }
  
  PP(pp_gt)
  {
!     dSP; dPOPnv;
!     SETs((TOPn > value) ? &sv_yes : &sv_no);
!     RETURN;
  }
  
  PP(pp_le)
  {
!     dSP; dPOPnv;
!     SETs((TOPn <= value) ? &sv_yes : &sv_no);
!     RETURN;
  }
  
  PP(pp_ge)
  {
!     dSP; dPOPnv;
!     SETs((TOPn >= value) ? &sv_yes : &sv_no);
!     RETURN;
  }
  
  PP(pp_eq)
  {
!     dSP; dPOPnv;
!     SETs((TOPn == value) ? &sv_yes : &sv_no);
!     RETURN;
  }
  
  PP(pp_ne)
  {
!     dSP; dPOPnv;
!     SETs((TOPn != value) ? &sv_yes : &sv_no);
!     RETURN;
  }
  
  PP(pp_ncmp)
  {
!     dSP; dTARGET; dPOPTOPnnrl;
!     I32 value;
  
!     if (left > right)
  	value = 1;
!     else if (left < right)
  	value = -1;
!     else
  	value = 0;
!     SETi(value);
!     RETURN;
  }
  
  PP(pp_slt)
  {
!     dSP; dPOPTOPssrl;
!     SETs( sv_cmp(left, right) < 0 ? &sv_yes : &sv_no );
!     RETURN;
  }
  
  PP(pp_sgt)
  {
!     dSP; dPOPTOPssrl;
!     SETs( sv_cmp(left, right) > 0 ? &sv_yes : &sv_no );
!     RETURN;
  }
  
  PP(pp_sle)
  {
!     dSP; dPOPTOPssrl;
!     SETs( sv_cmp(left, right) <= 0 ? &sv_yes : &sv_no );
!     RETURN;
  }
  
  PP(pp_sge)
  {
!     dSP; dPOPTOPssrl;
!     SETs( sv_cmp(left, right) >= 0 ? &sv_yes : &sv_no );
!     RETURN;
  }
  
  PP(pp_seq)
  {
!     dSP; dPOPTOPssrl;
!     SETs( sv_eq(left, right) ? &sv_yes : &sv_no );
!     RETURN;
  }
  
  PP(pp_sne)
  {
!     dSP; dPOPTOPssrl;
!     SETs( !sv_eq(left, right) ? &sv_yes : &sv_no );
!     RETURN;
  }
  
  PP(pp_scmp)
  {
!     dSP; dTARGET;
!     dPOPTOPssrl;
!     SETi( sv_cmp(left, right) );
!     RETURN;
  }
  
  PP(pp_bit_and) {
!     dSP; dATARGET; dPOPTOPssrl;
!     if (SvNIOK(left) || SvNIOK(right)) {
  	unsigned long value = U_L(SvNV(left));
  	value = value & U_L(SvNV(right));
  	SETn((double)value);
!     }
!     else {
  	do_vop(op->op_type, TARG, left, right);
  	SETTARG;
      }
-     RETURN;
  }
  
  PP(pp_bit_xor)
  {
!     dSP; dATARGET; dPOPTOPssrl;
!     if (SvNIOK(left) || SvNIOK(right)) {
  	unsigned long value = U_L(SvNV(left));
  	value = value ^ U_L(SvNV(right));
  	SETn((double)value);
!     }
!     else {
  	do_vop(op->op_type, TARG, left, right);
  	SETTARG;
      }
-     RETURN;
  }
  
  PP(pp_bit_or)
  {
!     dSP; dATARGET; dPOPTOPssrl;
!     if (SvNIOK(left) || SvNIOK(right)) {
  	unsigned long value = U_L(SvNV(left));
  	value = value | U_L(SvNV(right));
  	SETn((double)value);
!     }
!     else {
  	do_vop(op->op_type, TARG, left, right);
  	SETTARG;
      }
-     RETURN;
  }
  
  PP(pp_negate)
  {
!     dSP; dTARGET;
      SETn(-TOPn);
      RETURN;
  }
  
  PP(pp_not)
  {
      *stack_sp = SvTRUE(*stack_sp) ? &sv_no : &sv_yes;
      return NORMAL;
  }
  
  PP(pp_complement)
  {
!     dSP; dTARGET; dTOPss;
!     register I32 anum;
  
!     if (SvNIOK(sv)) {
  	SETi(  ~SvIV(sv) );
!     }
!     else {
  	register char *tmps;
  	register long *tmpl;
  	STRLEN len;
--- 1830,2079 ----
  
  PP(pp_left_shift)
  {
!     dSP; dATARGET; tryAMAGICbin(lshift,opASSIGN); 
!     {
!       dPOPTOPiirl;
!       SETi( left << right );
!       RETURN;
!     }
  }
  
  PP(pp_right_shift)
  {
!     dSP; dATARGET; tryAMAGICbin(rshift,opASSIGN); 
!     {
!       dPOPTOPiirl;
!       SETi( left >> right );
!       RETURN;
!     }
  }
  
  PP(pp_lt)
  {
!     dSP; tryAMAGICbinSET(lt,0); 
!     {
!       dPOPnv;
!       SETs((TOPn < value) ? &sv_yes : &sv_no);
!       RETURN;
!     }
  }
  
  PP(pp_gt)
  {
!     dSP; tryAMAGICbinSET(gt,0); 
!     {
!       dPOPnv;
!       SETs((TOPn > value) ? &sv_yes : &sv_no);
!       RETURN;
!     }
  }
  
  PP(pp_le)
  {
!     dSP; tryAMAGICbinSET(le,0); 
!     {
!       dPOPnv;
!       SETs((TOPn <= value) ? &sv_yes : &sv_no);
!       RETURN;
!     }
  }
  
  PP(pp_ge)
  {
!     dSP; tryAMAGICbinSET(ge,0); 
!     {
!       dPOPnv;
!       SETs((TOPn >= value) ? &sv_yes : &sv_no);
!       RETURN;
!     }
  }
  
  PP(pp_eq)
  {
!     dSP; tryAMAGICbinSET(eq,0); 
!     {
!       dPOPnv;
!       SETs((TOPn == value) ? &sv_yes : &sv_no);
!       RETURN;
!     }
  }
  
  PP(pp_ne)
  {
!     dSP; tryAMAGICbinSET(ne,0); 
!     {
!       dPOPnv;
!       SETs((TOPn != value) ? &sv_yes : &sv_no);
!       RETURN;
!     }
  }
  
  PP(pp_ncmp)
  {
!     dSP; dTARGET; tryAMAGICbin(ncmp,0); 
!     {
!       dPOPTOPnnrl;
!       I32 value;
  
!       if (left > right)
  	value = 1;
!       else if (left < right)
  	value = -1;
!       else
  	value = 0;
!       SETi(value);
!       RETURN;
!     }
  }
  
  PP(pp_slt)
  {
!     dSP; tryAMAGICbinSET(slt,0); 
!     {
!       dPOPTOPssrl;
!       SETs( sv_cmp(left, right) < 0 ? &sv_yes : &sv_no );
!       RETURN;
!     }
  }
  
  PP(pp_sgt)
  {
!     dSP; tryAMAGICbinSET(sgt,0); 
!     {
!       dPOPTOPssrl;
!       SETs( sv_cmp(left, right) > 0 ? &sv_yes : &sv_no );
!       RETURN;
!     }
  }
  
  PP(pp_sle)
  {
!     dSP; tryAMAGICbinSET(sle,0); 
!     {
!       dPOPTOPssrl;
!       SETs( sv_cmp(left, right) <= 0 ? &sv_yes : &sv_no );
!       RETURN;
!     }
  }
  
  PP(pp_sge)
  {
!     dSP; tryAMAGICbinSET(sge,0); 
!     {
!       dPOPTOPssrl;
!       SETs( sv_cmp(left, right) >= 0 ? &sv_yes : &sv_no );
!       RETURN;
!     }
  }
  
  PP(pp_seq)
  {
!     dSP; tryAMAGICbinSET(seq,0); 
!     {
!       dPOPTOPssrl;
!       SETs( sv_eq(left, right) ? &sv_yes : &sv_no );
!       RETURN;
!     }
  }
  
  PP(pp_sne)
  {
!     dSP; tryAMAGICbinSET(sne,0); 
!     {
!       dPOPTOPssrl;
!       SETs( !sv_eq(left, right) ? &sv_yes : &sv_no );
!       RETURN;
!     }
  }
  
  PP(pp_scmp)
  {
!     dSP; dTARGET;  tryAMAGICbin(scmp,0);
!     {
!       dPOPTOPssrl;
!       SETi( sv_cmp(left, right) );
!       RETURN;
!     }
  }
  
  PP(pp_bit_and) {
!     dSP; dATARGET; tryAMAGICbin(band,opASSIGN); 
!     {
!       dPOPTOPssrl;
!       if (SvNIOK(left) || SvNIOK(right)) {
  	unsigned long value = U_L(SvNV(left));
  	value = value & U_L(SvNV(right));
  	SETn((double)value);
!       }
!       else {
  	do_vop(op->op_type, TARG, left, right);
  	SETTARG;
+       }
+       RETURN;
      }
  }
  
  PP(pp_bit_xor)
  {
!     dSP; dATARGET; tryAMAGICbin(bxor,opASSIGN); 
!     {
!       dPOPTOPssrl;
!       if (SvNIOK(left) || SvNIOK(right)) {
  	unsigned long value = U_L(SvNV(left));
  	value = value ^ U_L(SvNV(right));
  	SETn((double)value);
!       }
!       else {
  	do_vop(op->op_type, TARG, left, right);
  	SETTARG;
+       }
+       RETURN;
      }
  }
  
  PP(pp_bit_or)
  {
!     dSP; dATARGET; tryAMAGICbin(bor,opASSIGN); 
!     {
!       dPOPTOPssrl;
!       if (SvNIOK(left) || SvNIOK(right)) {
  	unsigned long value = U_L(SvNV(left));
  	value = value | U_L(SvNV(right));
  	SETn((double)value);
!       }
!       else {
  	do_vop(op->op_type, TARG, left, right);
  	SETTARG;
+       }
+       RETURN;
      }
  }
  
  PP(pp_negate)
  {
!     dSP; dTARGET; tryAMAGICun(neg);
      SETn(-TOPn);
      RETURN;
  }
  
  PP(pp_not)
  {
+     dSP; tryAMAGICunSET(not);
      *stack_sp = SvTRUE(*stack_sp) ? &sv_no : &sv_yes;
      return NORMAL;
  }
  
  PP(pp_complement)
  {
!     dSP; dTARGET; tryAMAGICun(compl); 
!     {
!       dTOPss;
!       register I32 anum;
  
!       if (SvNIOK(sv)) {
  	SETi(  ~SvIV(sv) );
!       }
!       else {
  	register char *tmps;
  	register long *tmpl;
  	STRLEN len;
