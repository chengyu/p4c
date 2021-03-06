#include <core.p4>
#include <v1model.p4>

typedef standard_metadata_t std_meta_t;
header ipv4_t {
}

struct H {
    ipv4_t ipv4;
}

struct M {
}

parser ParserI(packet_in pk, out H hdr, inout M meta, inout std_meta_t std_meta) {
    state start {
        transition accept;
    }
}

control VerifyChecksumI(in H hdr, inout M meta) {
    apply {
    }
}

control ComputeChecksumI(inout H hdr, inout M meta) {
    apply {
    }
}

control IngressI(inout H hdr, inout M meta, inout std_meta_t std_meta) {
    ipv4_t tmp_ipv4;
    @name("NoAction_1") action NoAction_0() {
    }
    @name("do_aux.adjust_lkp_fields") table do_aux_adjust_lkp_fields_0() {
        key = {
            tmp_ipv4.isValid(): exact;
        }
        actions = {
            NoAction_0();
        }
        default_action = NoAction_0();
    }
    action act() {
        tmp_ipv4 = hdr.ipv4;
    }
    table tbl_act() {
        actions = {
            act();
        }
        const default_action = act();
    }
    apply {
        tbl_act.apply();
        do_aux_adjust_lkp_fields_0.apply();
    }
}

control EgressI(inout H hdr, inout M meta, inout std_meta_t std_meta) {
    apply {
    }
}

control DeparserI(packet_out b, in H hdr) {
    apply {
    }
}

V1Switch<H, M>(ParserI(), VerifyChecksumI(), IngressI(), EgressI(), ComputeChecksumI(), DeparserI()) main;
