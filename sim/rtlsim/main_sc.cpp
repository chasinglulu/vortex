/* SPDX-License-Identifier: GPL-2.0
 *
 * Vortex AXI GPGPU SystemC TLM-2.0 Wrapper.
 *
 * Copyright (C) 2024, Charleye<wangkart@aliyun.com>
 * All rights reserved.
 *
 */

#include <iostream>
#include <systemc>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>

#include "VVortex_axi.h"
#include "trace.h"

#include "tlm-bridges/axi2tlm-bridge.h"

using namespace sc_core;
using namespace sc_dt;
using namespace std;

#ifndef TRACE_START_TIME
#define TRACE_START_TIME 0ull
#endif

#ifndef TRACE_STOP_TIME
#define TRACE_STOP_TIME -1ull
#endif

static uint64_t timestamp = 0;

double sc_time_stamp() {
	return timestamp;
}

static bool trace_enabled = false;
static uint64_t trace_start_time = TRACE_START_TIME;
static uint64_t trace_stop_time = TRACE_STOP_TIME;

bool sim_trace_enabled() {
	if (timestamp >= trace_start_time
			&& timestamp < trace_stop_time)
		return true;

	return trace_enabled;
}

void sim_trace_enable(bool enable) {
	trace_enabled = enable;
}

#ifdef XLEN_32
#define XLEN 32
#endif

#ifdef XLEN_64
#define XLEN 64
#endif

#ifndef XLEN_32
#ifndef XLEN_64
#define XLEN 32
#endif
#endif

SC_MODULE(Processor)
{
	SC_HAS_PROCESS(Processor);
	VVortex_axi *device;

	axi2tlm_bridge<32, 512, 10, 8, 2> axi2tlm;

	// Clock
	sc_clock *clk;
	sc_signal<bool> rst, reset;

	// AXI write request address channel
	sc_signal<bool> m_axi_awvalid;
	sc_signal<bool> m_axi_awready;
	sc_signal<sc_bv<XLEN> > m_axi_awaddr;
	sc_signal<sc_bv<10> > m_axi_awid;
	sc_signal<sc_bv<8> > m_axi_awlen;
	sc_signal<sc_bv<3> > m_axi_awsize;
	sc_signal<sc_bv<2> > m_axi_awburst;
	sc_signal<sc_bv<2> > m_axi_awlock;
	sc_signal<sc_bv<4> > m_axi_awcache;
	sc_signal<sc_bv<3> > m_axi_awprot;
	sc_signal<sc_bv<4> > m_axi_awqos;
	sc_signal<sc_bv<4> > m_axi_awregion;

	// AXI write request data channel
	sc_signal<bool> m_axi_wvalid;
	sc_signal<bool> m_axi_wready;
	sc_signal<sc_bv<XLEN*16> > m_axi_wdata;
	sc_signal<sc_bv<64> > m_axi_wstrb;
	sc_signal<bool> m_axi_wlast;

	// AXI write response channel
	sc_signal<bool> m_axi_bvalid;
	sc_signal<bool> m_axi_bready;
	sc_signal<sc_bv<10> > m_axi_bid;
	sc_signal<sc_bv<2> > m_axi_bresp;

	// AXI read request channel
	sc_signal<bool> m_axi_arvalid;
	sc_signal<bool> m_axi_arready;
	sc_signal<sc_bv<XLEN> > m_axi_araddr;
	sc_signal<sc_bv<10> > m_axi_arid;
	sc_signal<sc_bv<8> > m_axi_arlen;
	sc_signal<sc_bv<3> > m_axi_arsize;
	sc_signal<sc_bv<2> > m_axi_arburst;
	sc_signal<sc_bv<2> > m_axi_arlock;
	sc_signal<sc_bv<4> > m_axi_arcache;
	sc_signal<sc_bv<3> > m_axi_arprot;
	sc_signal<sc_bv<4> > m_axi_arqos;
	sc_signal<sc_bv<4> > m_axi_arregion;

	// AXI read response channel
	sc_signal<bool> m_axi_rvalid;
	sc_signal<bool> m_axi_rready;
	sc_signal<sc_bv<XLEN*16> > m_axi_rdata;
	sc_signal<bool> m_axi_rlast;
	sc_signal<sc_bv<10> > m_axi_rid;
	sc_signal<sc_bv<2> > m_axi_rresp;

	// DCR write request
	sc_signal<bool> dcr_wr_valid;
	sc_signal<sc_bv<12> > dcr_wr_addr;
	sc_signal<sc_bv<XLEN> > dcr_wr_data;

	// Status
	sc_signal<bool> busy;

	void gen_rst_n(void)
	{
		reset.write(!rst.read());
	}

	Processor(sc_module_name name) :
		axi2tlm("axi2tlm"),
		rst("rst"),
		reset("reset"),
		m_axi_awvalid("m_axi_awvalid"),
		m_axi_awready("m_axi_awready"),
		m_axi_awaddr("m_axi_awaddr"),
		m_axi_awid("m_axi_awid"),
		m_axi_awlen("m_axi_awlen"),
		m_axi_awsize("m_axi_awsize"),
		m_axi_awburst("m_axi_awburst"),
		m_axi_awlock("m_axi_awlock"),
		m_axi_awcache("m_axi_awcache"),
		m_axi_awprot("m_axi_awprot"),
		m_axi_awqos("m_axi_awqos"),
		m_axi_awregion("m_axi_awregion"),
		m_axi_wvalid("m_axi_wvalid"),
		m_axi_wready("m_axi_wready"),
		m_axi_wdata("m_axi_wdata"),
		m_axi_wstrb("m_axi_wstrb"),
		m_axi_wlast("m_axi_wlast"),
		m_axi_bvalid("m_axi_bvalid"),
		m_axi_bready("m_axi_bready"),
		m_axi_bid("m_axi_bid"),
		m_axi_bresp("m_axi_bresp"),
		m_axi_arvalid("m_axi_arvalid"),
		m_axi_arready("m_axi_arready"),
		m_axi_araddr("m_axi_araddr"),
		m_axi_arid("m_axi_arid"),
		m_axi_arlen("m_axi_arlen"),
		m_axi_arsize("m_axi_arsize"),
		m_axi_arburst("m_axi_arburst"),
		m_axi_arlock("m_axi_arlock"),
		m_axi_arcache("m_axi_arcache"),
		m_axi_arprot("m_axi_arprot"),
		m_axi_arqos("m_axi_arqos"),
		m_axi_arregion("m_axi_arregion"),
		m_axi_rvalid("m_axi_rvalid"),
		m_axi_rready("m_axi_rready"),
		m_axi_rdata("m_axi_rdata"),
		m_axi_rlast("m_axi_rlast"),
		m_axi_rid("m_axi_rid"),
		m_axi_rresp("m_axi_rresp"),
		dcr_wr_valid("dcr_wr_valid"),
		dcr_wr_addr("dcr_wr_addr"),
		dcr_wr_data("dcr_wr_data"),
		busy("busy")
	{
		SC_METHOD(gen_rst_n);
		sensitive << rst;

		/* Slow clock to keep simulation fast.  */
		clk = new sc_clock("clk", sc_time(10, SC_US));
		device = new VVortex_axi("gpgpu");

		device->clk(*clk);
		device->reset(reset);
		device->m_axi_awvalid[0](m_axi_awvalid);
		device->m_axi_awready[0](m_axi_awready);
		device->m_axi_awaddr[0](m_axi_awaddr);
		device->m_axi_awid[0](m_axi_awid);
		device->m_axi_awlen[0](m_axi_awlen);
		device->m_axi_awsize[0](m_axi_awsize);
		device->m_axi_awburst[0](m_axi_awburst);
		device->m_axi_awlock[0](m_axi_awlock);
		device->m_axi_awcache[0](m_axi_awcache);
		device->m_axi_awprot[0](m_axi_awprot);
		device->m_axi_awqos[0](m_axi_awqos);
		device->m_axi_awregion[0](m_axi_awregion);

		device->m_axi_wvalid[0](m_axi_wvalid);
		device->m_axi_wready[0](m_axi_wready);
		device->m_axi_wdata[0](m_axi_wdata);
		device->m_axi_wstrb[0](m_axi_wstrb);
		device->m_axi_wlast[0](m_axi_wlast);

		device->m_axi_bvalid[0](m_axi_bvalid);
		device->m_axi_bready[0](m_axi_bready);
		device->m_axi_bid[0](m_axi_bid);
		device->m_axi_bresp[0](m_axi_bresp);

		device->m_axi_arvalid[0](m_axi_arvalid);
		device->m_axi_arready[0](m_axi_arready);
		device->m_axi_araddr[0](m_axi_araddr);
		device->m_axi_arid[0](m_axi_arid);
		device->m_axi_arlen[0](m_axi_arlen);
		device->m_axi_arsize[0](m_axi_arsize);
		device->m_axi_arburst[0](m_axi_arburst);
		device->m_axi_arlock[0](m_axi_arlock);
		device->m_axi_arcache[0](m_axi_arcache);
		device->m_axi_arprot[0](m_axi_arprot);
		device->m_axi_arqos[0](m_axi_arqos);
		device->m_axi_arregion[0](m_axi_arregion);

		device->m_axi_rvalid[0](m_axi_rvalid);
		device->m_axi_rready[0](m_axi_rready);
		device->m_axi_rdata[0](m_axi_rdata);
		device->m_axi_rlast[0](m_axi_rlast);
		device->m_axi_rid[0](m_axi_rid);
		device->m_axi_rresp[0](m_axi_rresp);

		device->dcr_wr_valid(dcr_wr_valid);
		device->dcr_wr_addr(dcr_wr_addr);
		device->dcr_wr_data(dcr_wr_data);

		device->busy(busy);

		axi2tlm.clk(*clk);
		axi2tlm.resetn(reset);
		axi2tlm.awvalid(m_axi_awvalid);
		axi2tlm.awready(m_axi_awready);
		axi2tlm.awaddr(m_axi_awaddr);
		axi2tlm.awid(m_axi_awid);
		axi2tlm.awlen(m_axi_awlen);
		axi2tlm.awsize(m_axi_awsize);
		axi2tlm.awburst(m_axi_awburst);
		axi2tlm.awlock(m_axi_awlock);
		axi2tlm.awcache(m_axi_awcache);
		axi2tlm.awprot(m_axi_awprot);
		axi2tlm.awqos(m_axi_awqos);
		axi2tlm.awregion(m_axi_awregion);

		axi2tlm.wvalid(m_axi_wvalid);
		axi2tlm.wready(m_axi_wready);
		axi2tlm.wdata(m_axi_wdata);
		axi2tlm.wstrb(m_axi_wstrb);
		axi2tlm.wlast(m_axi_wlast);

		axi2tlm.bvalid(m_axi_bvalid);
		axi2tlm.bready(m_axi_bready);
		axi2tlm.bid(m_axi_bid);
		axi2tlm.bresp(m_axi_bresp);

		axi2tlm.arvalid(m_axi_arvalid);
		axi2tlm.arready(m_axi_arready);
		axi2tlm.araddr(m_axi_araddr);
		axi2tlm.arid(m_axi_arid);
		axi2tlm.arlen(m_axi_arlen);
		axi2tlm.arsize(m_axi_arsize);
		axi2tlm.arburst(m_axi_arburst);
		axi2tlm.arlock(m_axi_arlock);
		axi2tlm.arcache(m_axi_arcache);
		axi2tlm.arprot(m_axi_arprot);
		axi2tlm.arqos(m_axi_arqos);
		axi2tlm.arregion(m_axi_arregion);

		axi2tlm.rvalid(m_axi_rvalid);
		axi2tlm.rready(m_axi_rready);
		axi2tlm.rdata(m_axi_rdata);
		axi2tlm.rlast(m_axi_rlast);
		axi2tlm.rid(m_axi_rid);
		axi2tlm.rresp(m_axi_rresp);
	}
};

int sc_main(int argc, char **argv)
{
	Processor *vortex;
	sc_trace_file *trace_fp = NULL;

	Verilated::commandArgs(argc, argv);
	sc_set_time_resolution(1, SC_PS);

	vortex = new Processor("vortex");

	cout << "start vortex" << endl;

	trace_fp = sc_create_vcd_trace_file("trace");
	trace(trace_fp, *vortex, vortex->name());
	/* Pull the reset signal.  */
	vortex->rst.write(true);
	sc_start(1, SC_US);
	vortex->rst.write(false);

	cout << "reset" << endl;

	sc_start();
	if (trace_fp) {
		sc_close_vcd_trace_file(trace_fp);
	}

	return 0;
}