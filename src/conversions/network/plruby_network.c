#include "convcommon.h"
#include <utils/inet.h>

static void pl_inet_mark(inet *p) {}

static VALUE
pl_inet_s_alloc(VALUE obj)
{
    void *v;
    inet *inst;

    v = (void *)PLRUBY_DFC1(inet_in, "0.0.0.0");
    inst = (inet *)ALLOC_N(char, VARSIZE(v));
    CPY_FREE(inst, v, VARSIZE(v));
    return Data_Wrap_Struct(obj, pl_inet_mark, free, inst);
}

static VALUE
pl_inet_init_copy(VALUE copy, VALUE orig)
{
    inet *t0, *t1;
    int s0, s1;
    
    if (copy == orig) return copy;
    if (TYPE(orig) != T_DATA ||
        RDATA(orig)->dmark != (RUBY_DATA_FUNC) pl_inet_mark) {
        rb_raise(rb_eTypeError, "wrong argument type to clone");
    }
    Data_Get_Struct(orig, inet, t0);
    Data_Get_Struct(copy, inet, t1);
    s0 = VARSIZE(t0);
    s1 = VARSIZE(t1);
    if (s0 != s1) {
        free(t1);
        DATA_PTR(copy) = 0;
        t1 = (inet *)ALLOC_N(char, s0);
        DATA_PTR(copy) = t1;
    }
    memcpy(t1, t0, s0);
    return copy;
}

static VALUE
pl_inet_s_datum(VALUE obj, VALUE a)
{
    inet *ip0, *ip1;
    Oid typoid;
    VALUE res;

    ip0 = (inet *)plruby_datum_get(a, &typoid);
    if (typoid != INETOID && typoid != CIDROID) {
	rb_raise(rb_eArgError, "unknown OID type %d", typoid);
    }
    ip1 = (inet *)ALLOC_N(char, VARSIZE(ip0));
    memcpy(ip1, ip0, VARSIZE(ip0));
    res = Data_Wrap_Struct(obj, pl_inet_mark, free, ip1);
    OBJ_TAINT(res);
    return res;
}

static VALUE
pl_inet_to_datum(VALUE obj, VALUE a)
{
    inet *ip0, *ip1;
    Oid typoid;

    typoid = plruby_datum_oid(a, 0);
    if (typoid != INETOID && typoid != CIDROID) {
	return Qnil;
    }
    Data_Get_Struct(obj, inet, ip0);
    ip1 = (inet *)palloc(VARSIZE(ip0));
    memcpy(ip1, ip0, VARSIZE(ip0));
    return plruby_datum_set(a, (Datum)ip1);
}

PL_MLOADVAR(pl_inet_mload, inet_recv, inet, VARSIZE);
PL_MDUMP(pl_inet_mdump, inet_send);

static VALUE
pl_inet_init(int argc, VALUE *argv, VALUE obj)
{
    inet *inst;
    void *v;
    VALUE a, b;
    int cidr = 0;

    if (rb_scan_args(argc, argv, "11", &a, &b) == 2) {
	cidr = RTEST(b);
    }
    a = plruby_to_s(a);
    Data_Get_Struct(obj, inet, inst);
    if (cidr) {
	v = (void *)PLRUBY_DFC1(cidr_in, RSTRING_PTR(a));
    }
    else {
	v = (void *)PLRUBY_DFC1(inet_in, RSTRING_PTR(a));
    }
    free(inst);
    inst = (inet *)ALLOC_N(char, VARSIZE(v));
    CPY_FREE(inst, v, VARSIZE(v));
    RDATA(obj)->data = inst;
    return obj;
}

static VALUE
pl_inet_cmp(VALUE a, VALUE b)
{
    inet *inst0, *inst1;

    if (!rb_obj_is_kind_of(b, rb_obj_class(a))) {
	return Qnil;
    }
    Data_Get_Struct(a, inet, inst0);
    Data_Get_Struct(b, inet, inst1);
    if (PLRUBY_DFC2(network_eq, inst0, inst1)) return INT2NUM(0);
    if (PLRUBY_DFC2(network_lt, inst0, inst1)) return INT2NUM(-1);
    return INT2FIX(1);
}

#define NETWORK_BOOL(NAME_, FUNCTION_)                          \
static VALUE                                                    \
NAME_(VALUE obj, VALUE a)                                       \
{                                                               \
    inet *inst0, *inst1;                                        \
                                                                \
    if (!rb_obj_is_kind_of(a, rb_obj_class(obj))) {             \
	rb_raise(rb_eArgError, "expected a NetAddr object");    \
    }                                                           \
    Data_Get_Struct(obj, inet, inst0);                          \
    Data_Get_Struct(a, inet, inst1);                            \
    if (PLRUBY_DFC2(FUNCTION_, inst0, inst1)) return Qtrue;     \
    return Qfalse;                                              \
}

NETWORK_BOOL(pl_inet_contained, network_sub);
NETWORK_BOOL(pl_inet_containedeq, network_subeq);
NETWORK_BOOL(pl_inet_contain, network_sup);
NETWORK_BOOL(pl_inet_containeq, network_supeq);

#define NETWORK_CALL(NAME_,FUNCTION_)				\
static VALUE							\
NAME_(VALUE obj)						\
{								\
    inet *src;							\
    char *str;							\
    VALUE res;							\
								\
    Data_Get_Struct(obj, inet, src);				\
    str = (char *)PLRUBY_DFC1(FUNCTION_, src);			\
    if (OBJ_TAINTED(obj)) {					\
	res = rb_tainted_str_new((char *)VARDATA(str),		\
				 VARSIZE(str) - VARHDRSZ);	\
    }								\
    else {							\
	res = rb_str_new((char *)VARDATA(str),			\
			 VARSIZE(str) - VARHDRSZ);		\
    }								\
    pfree(str);							\
    return res;							\
}

NETWORK_CALL(pl_inet_host, network_host);

NETWORK_CALL(pl_inet_abbrev, inet_abbrev);

static VALUE
pl_inet_to_s(VALUE obj)
{
    inet *src;
    char *str;
    VALUE res;

    Data_Get_Struct(obj, inet, src);
    str = (char *)PLRUBY_DFC1(inet_out, src);
    if (OBJ_TAINTED(obj)) {
	res = rb_tainted_str_new2(str);
    }
    else {
	res = rb_str_new2(str);
    }
    pfree(str);
    return res;
}

static VALUE
pl_inet_masklen(VALUE obj)
{
    inet *src;
    Data_Get_Struct(obj, inet, src);
    return INT2NUM(DatumGetInt32(PLRUBY_DFC1(network_masklen, src)));
}

static VALUE
pl_inet_setmasklen(VALUE obj, VALUE a)
{
    inet *s0, *s1, *s2;
    VALUE res;

    Data_Get_Struct(obj, inet, s0);
    s1 = (inet *)PLRUBY_DFC2(inet_set_masklen, s0, Int32GetDatum(NUM2INT(a)));
    s2 = (inet *)ALLOC_N(char, VARSIZE(s1));
    CPY_FREE(s2, s1, VARSIZE(s1));
    res = Data_Wrap_Struct(rb_obj_class(obj), pl_inet_mark, free, s2);
    if (OBJ_TAINTED(res) || OBJ_TAINTED(a)) OBJ_TAINT(res);
    return res;
}

static VALUE
pl_inet_family(VALUE obj)
{
    inet *s;
    VALUE str;

    Data_Get_Struct(obj, inet, s);
    switch (DatumGetInt32(PLRUBY_DFC1(network_family, s))) {
    case 4:
	str = rb_str_new2("AF_INET");
	break;
    case 6:
	str = rb_str_new2("AF_INET6");
	break;
    default:
	str = Qnil;
	break;
    }
    if (OBJ_TAINTED(obj)) OBJ_TAINT(str);
    return str;
}

#define NETWORK_INET(NAME_, FUNCTION_)					\
static VALUE								\
NAME_(VALUE obj)							\
{									\
    inet *ip0, *ip1, *ip2;						\
    VALUE res;								\
									\
    Data_Get_Struct(obj, inet, ip0);					\
    res = Data_Make_Struct(rb_obj_class(obj), inet,			\
			   pl_inet_mark, free, ip1);			\
    ip2 = (inet *)PLRUBY_DFC1(FUNCTION_, ip0);                          \
    ip1 = (inet *)ALLOC_N(char, VARSIZE(ip2));				\
    CPY_FREE(ip1, ip2, VARSIZE(ip2));					\
    res = Data_Wrap_Struct(rb_obj_class(obj), pl_inet_mark, free, ip1);	\
    if (OBJ_TAINTED(obj)) OBJ_TAINT(res);				\
    return res;								\
}

NETWORK_INET(pl_inet_broadcast, network_broadcast);
NETWORK_INET(pl_inet_network, network_network);
NETWORK_INET(pl_inet_netmask, network_netmask);
NETWORK_INET(pl_inet_hostmask, network_hostmask);

static VALUE
pl_inet_last(VALUE obj)
{
    inet *ip0, *ip1, *ip2;
    VALUE res;

    Data_Get_Struct(obj, inet, ip0);
    res = Data_Make_Struct(rb_obj_class(obj), inet,
			   pl_inet_mark, free, ip1);
    ip2 = (inet *)network_scan_last((Datum)ip0);
    ip1 = (inet *)ALLOC_N(char, VARSIZE(ip2));
    CPY_FREE(ip1, ip2, VARSIZE(ip2));
    res = Data_Wrap_Struct(rb_obj_class(obj), pl_inet_mark, free, ip1);
    if (OBJ_TAINTED(obj)) OBJ_TAINT(res);
    return res;
}

static VALUE
pl_inet_s_caddr(VALUE obj)
{
    inet *ip0, *ip1;
    VALUE res;

    ip0 = (inet *)PLRUBY_DFC0(inet_client_addr);
    if (!ip0) {
        return Qnil;
    }
    ip1 = (inet *)ALLOC_N(char, VARSIZE(ip0));
    CPY_FREE(ip1, ip0, VARSIZE(ip0));
    res = Data_Wrap_Struct(obj, pl_inet_mark, free, ip1);
    if (OBJ_TAINTED(obj)) OBJ_TAINT(res);
    return res;
}

static VALUE
pl_inet_s_cport(VALUE obj)
{
    return INT2NUM(PLRUBY_DFC0(inet_client_port));
}

static VALUE
pl_inet_s_saddr(VALUE obj)
{
    inet *ip0, *ip1;
    VALUE res;

    ip0 = (inet *)PLRUBY_DFC0(inet_server_addr);
    if (!ip0) {
        return Qnil;
    }
    ip1 = (inet *)ALLOC_N(char, VARSIZE(ip0));
    CPY_FREE(ip1, ip0, VARSIZE(ip0));
    res = Data_Wrap_Struct(obj, pl_inet_mark, free, ip1);
    if (OBJ_TAINTED(obj)) OBJ_TAINT(res);
    return res;
}

static VALUE
pl_inet_s_sport(VALUE obj)
{
    return INT2NUM(PLRUBY_DFC0(inet_server_port));
}

static void pl_mac_mark(macaddr *mac) {}

static VALUE
pl_mac_s_alloc(VALUE obj)
{
    macaddr *mac;
    return Data_Make_Struct(obj, macaddr, pl_mac_mark, free, mac);
}

static VALUE
pl_mac_init_copy(VALUE copy, VALUE orig)
{
    macaddr *t0, *t1;

    if (copy == orig) return copy;
    if (TYPE(orig) != T_DATA ||
        RDATA(orig)->dmark != (RUBY_DATA_FUNC)pl_mac_mark) {
        rb_raise(rb_eTypeError, "wrong argument type to clone");
    }
    Data_Get_Struct(orig, macaddr, t0);
    Data_Get_Struct(copy, macaddr, t1);
    MEMCPY(t1, t0, macaddr, 1);
    return copy;
}

static VALUE
pl_mac_s_datum(VALUE obj, VALUE a)
{
    macaddr *mac0, *mac1;
    Oid typoid;
    VALUE res;

    mac0 = (macaddr *)plruby_datum_get(a, &typoid);
    if (typoid != MACADDROID) {
	rb_raise(rb_eArgError, "unknown OID type %d", typoid);
    }
    mac1 = ALLOC_N(macaddr, 1);
    memcpy(mac1, mac0, sizeof(macaddr));
    res = Data_Wrap_Struct(obj, pl_mac_mark, free, mac1);
    OBJ_TAINT(res);
    return res;
}

static VALUE
pl_mac_to_datum(VALUE obj, VALUE a)
{
    macaddr *mac0, *mac1;
    Oid typoid;

    typoid = plruby_datum_oid(a, 0);
    if (typoid != MACADDROID) {
	return Qnil;
    }
    Data_Get_Struct(obj, macaddr, mac0);
    mac1 = (macaddr *)palloc(sizeof(macaddr));
    memcpy(mac1, mac0, sizeof(macaddr));
    return plruby_datum_set(a, (Datum)mac1);
}

PL_MLOAD(pl_mac_mload, macaddr_recv, macaddr);
PL_MDUMP(pl_mac_mdump, macaddr_send);

static VALUE
pl_mac_init(VALUE obj, VALUE a)
{
    macaddr *m0, *m1;

    a = plruby_to_s(a);
    Data_Get_Struct(obj, struct macaddr, m0);
    m1 = (macaddr *)PLRUBY_DFC1(macaddr_in, RSTRING_PTR(a));
    CPY_FREE(m0, m1, sizeof(macaddr));
    return obj;
}

static VALUE
pl_mac_cmp(VALUE obj, VALUE a)
{
    macaddr *m0, *m1;
    int res;

    if (!rb_obj_is_kind_of(a, rb_obj_class(obj))) {
	return Qnil;
    }
    Data_Get_Struct(obj, macaddr, m0);
    Data_Get_Struct(a, macaddr, m1);
    res = DatumGetInt32(PLRUBY_DFC2(macaddr_cmp, m0, m1));
    return INT2NUM(res);
}

static VALUE
pl_mac_to_s(VALUE obj)
{
    macaddr *m;
    char *s;
    VALUE res;

    Data_Get_Struct(obj, macaddr, m);
    s = (char *)PLRUBY_DFC1(macaddr_out, m);
    res = rb_str_new2(s);
    pfree(s);
    if (OBJ_TAINTED(obj)) OBJ_TAINT(res);
    return res;
}

static VALUE
pl_mac_truncate(VALUE obj)
{
    macaddr *m0, *m1, *m2;
    VALUE res;

    Data_Get_Struct(obj, macaddr, m0);
    m2 = (macaddr *)PLRUBY_DFC1(macaddr_trunc, m0);
    res = Data_Make_Struct(rb_obj_class(obj), macaddr, pl_mac_mark, free, m1);
    CPY_FREE(m1, m2, sizeof(macaddr));
    if (OBJ_TAINTED(obj)) OBJ_TAINT(res);
    return res;
}

void Init_plruby_network()
{
    VALUE pl_cInet, pl_cMac;

    pl_cInet = rb_define_class("NetAddr", rb_cObject);
    rb_include_module(pl_cInet, rb_mComparable);
    rb_undef_method(CLASS_OF(pl_cInet), "method_missing");
#if HAVE_RB_DEFINE_ALLOC_FUNC
    rb_define_alloc_func(pl_cInet, pl_inet_s_alloc);
#else
    rb_define_singleton_method(pl_cInet, "allocate", pl_inet_s_alloc, 0);
#endif
    rb_define_singleton_method(pl_cInet, "new", plruby_s_new, -1);
    rb_define_singleton_method(pl_cInet, "from_string", plruby_s_new, -1);
    rb_define_singleton_method(pl_cInet, "from_datum", pl_inet_s_datum, 1);
    rb_define_singleton_method(pl_cInet, "client_addr", pl_inet_s_caddr, 0);
    rb_define_singleton_method(pl_cInet, "client_port", pl_inet_s_cport, 0);
    rb_define_singleton_method(pl_cInet, "server_addr", pl_inet_s_saddr, 0);
    rb_define_singleton_method(pl_cInet, "server_port", pl_inet_s_sport, 0);
    rb_define_method(pl_cInet, "to_datum", pl_inet_to_datum, 1);
    rb_define_method(pl_cInet, "initialize", pl_inet_init, -1);
#ifndef HAVE_RB_INITIALIZE_COPY
    rb_define_method(pl_cInet, "clone", plruby_clone, 0);
#endif
    rb_define_method(pl_cInet, "initialize_copy", pl_inet_init_copy, 1);
    rb_define_method(pl_cInet, "marshal_load", pl_inet_mload, 1);
    rb_define_method(pl_cInet, "marshal_dump", pl_inet_mdump, -1);
#ifndef RUBY_CAN_USE_MARSHAL_LOAD
    rb_define_singleton_method(pl_cInet, "_load", plruby_s_load, 1);
    rb_define_alias(pl_cInet, "_dump", "marshal_dump");
#endif
    rb_define_method(pl_cInet, "<=>", pl_inet_cmp, 1);
    rb_define_method(pl_cInet, "contained?", pl_inet_contained, 1);
    rb_define_method(pl_cInet, "contained_or_equal?", pl_inet_containedeq, 1);
    rb_define_method(pl_cInet, "contain?", pl_inet_contain, 1);
    rb_define_method(pl_cInet, "contain_or_equal?", pl_inet_containeq, 1);
    rb_define_method(pl_cInet, "host", pl_inet_host, 0);
    rb_define_method(pl_cInet, "abbrev", pl_inet_abbrev, 0);
    rb_define_method(pl_cInet, "masklen", pl_inet_masklen, 0);
    rb_define_method(pl_cInet, "set_masklen", pl_inet_setmasklen, 1);
    rb_define_method(pl_cInet, "family", pl_inet_family, 0);
    rb_define_method(pl_cInet, "broadcast", pl_inet_broadcast, 0);
    rb_define_method(pl_cInet, "network", pl_inet_network, 0);
    rb_define_method(pl_cInet, "netmask", pl_inet_netmask, 0);
    rb_define_method(pl_cInet, "hostmask", pl_inet_hostmask, 0);
    rb_define_method(pl_cInet, "to_s", pl_inet_to_s, 0);
    rb_define_method(pl_cInet, "first", pl_inet_network, 0);
    rb_define_method(pl_cInet, "last", pl_inet_last, 0);
    pl_cMac = rb_define_class("MacAddr", rb_cObject);
    rb_include_module(pl_cMac, rb_mComparable);
    rb_undef_method(CLASS_OF(pl_cMac), "method_missing");
#if HAVE_RB_DEFINE_ALLOC_FUNC
    rb_define_alloc_func(pl_cMac, pl_mac_s_alloc);
#else
    rb_define_singleton_method(pl_cMac, "allocate", pl_mac_s_alloc, 0);
#endif
    rb_define_singleton_method(pl_cMac, "new", plruby_s_new, -1);
    rb_define_singleton_method(pl_cMac, "from_string", plruby_s_new, -1);
    rb_define_singleton_method(pl_cMac, "from_datum", pl_mac_s_datum, 1);
    rb_define_method(pl_cMac, "to_datum", pl_mac_to_datum, 1);
    rb_define_method(pl_cMac, "initialize", pl_mac_init, 1);
#ifndef HAVE_RB_INITIALIZE_COPY
    rb_define_method(pl_cMac, "clone", plruby_clone, 0);
#endif
    rb_define_method(pl_cMac, "initialize_copy", pl_mac_init_copy, 1);
    rb_define_method(pl_cMac, "marshal_load", pl_mac_mload, 1);
    rb_define_method(pl_cMac, "marshal_dump", pl_mac_mdump, -1);
#ifndef RUBY_CAN_USE_MARSHAL_LOAD
    rb_define_singleton_method(pl_cMac, "_load", plruby_s_load, 1);
    rb_define_alias(pl_cMac, "_dump", "marshal_dump");
#endif
    rb_define_method(pl_cMac, "<=>", pl_mac_cmp, 1);
    rb_define_method(pl_cMac, "to_s", pl_mac_to_s, 0);
    rb_define_method(pl_cMac, "truncate", pl_mac_truncate, 0);
}
