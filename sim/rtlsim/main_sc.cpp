/* SPDX-License-Identifier: GPL-2.0
 *
 * Vortex AXI GPGPU SystemC TLM-2.0 Wrapper.
 *
 * Copyright (C) 2024, Charleye<wangkart@aliyun.com>
 * All rights reserved.
 *
 */

#include <iostream>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <systemc>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>

#include "VVortex_axi.h"
#include "tlm2dcr_bridge.h"
#include "trace.h"
#include "util.h"
#include "mem.h"

#include "tlm-bridges/axi2tlm-bridge.h"

using namespace sc_core;
using namespace sc_dt;
using namespace std;
using namespace vortex;

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

#define IO_COUT_ADDR (0xFF000000 + (1 << 14))
#define IO_COUT_SIZE 64

SC_MODULE(Processor)
{
	SC_HAS_PROCESS(Processor);

	VVortex_axi *device;
	RAM *ram_;

	uint64_t dcr_addr;
	uint64_t dcr_data;

	tlm_utils::simple_initiator_socket<Processor> dcr_interface;
	tlm_utils::simple_target_socket<Processor> m_axi;

	std::unordered_map<int, std::stringstream> print_bufs_;

#ifdef DEBUG
	axi2tlm_bridge<32, 512, 53, 8, 2> axi2tlm;
#else
	axi2tlm_bridge<32, 512, 10, 8, 2> axi2tlm;
#endif
	tlm2dcr_bridge<32> tlm2dcr;

	// Clock
	sc_clock *clk;
	sc_signal<bool> rst, reset_n;

	// AXI write request address channel
	sc_signal<bool> m_axi_awvalid;
	sc_signal<bool> m_axi_awready;
	sc_signal<sc_bv<XLEN> > m_axi_awaddr;
#ifdef DEBUG
	sc_signal<sc_bv<53> > m_axi_awid;
#else
	sc_signal<sc_bv<10> > m_axi_awid;
#endif
	sc_signal<sc_bv<8> > m_axi_awlen;
	sc_signal<sc_bv<3> > m_axi_awsize;
	sc_signal<sc_bv<2> > m_axi_awburst;
	sc_signal<sc_bv<2> > m_axi_awlock;
	sc_signal<sc_bv<4> > m_axi_awcache;
	sc_signal<sc_bv<3> > m_axi_awprot;
	sc_signal<sc_bv<4> > m_axi_awqos;
	sc_signal<sc_bv<4> > m_axi_awregion;
	sc_signal<sc_bv<2> > m_axi_awuser;

	// AXI write request data channel
	sc_signal<bool> m_axi_wvalid;
	sc_signal<bool> m_axi_wready;
	sc_signal<sc_bv<XLEN*16> > m_axi_wdata;
	sc_signal<sc_bv<64> > m_axi_wstrb;
	sc_signal<bool> m_axi_wlast;
	sc_signal<sc_bv<2> > m_axi_wuser;

	// AXI write response channel
	sc_signal<bool> m_axi_bvalid;
	sc_signal<bool> m_axi_bready;
#ifdef DEBUG
	sc_signal<sc_bv<53> > m_axi_bid;
#else
	sc_signal<sc_bv<10> > m_axi_bid;
#endif
	sc_signal<sc_bv<2> > m_axi_bresp;
	sc_signal<sc_bv<2> > m_axi_buser;

	// AXI read request channel
	sc_signal<bool> m_axi_arvalid;
	sc_signal<bool> m_axi_arready;
	sc_signal<sc_bv<XLEN> > m_axi_araddr;
#ifdef DEBUG
	sc_signal<sc_bv<53> > m_axi_arid;
#else
	sc_signal<sc_bv<10> > m_axi_arid;
#endif
	sc_signal<sc_bv<8> > m_axi_arlen;
	sc_signal<sc_bv<3> > m_axi_arsize;
	sc_signal<sc_bv<2> > m_axi_arburst;
	sc_signal<sc_bv<2> > m_axi_arlock;
	sc_signal<sc_bv<4> > m_axi_arcache;
	sc_signal<sc_bv<3> > m_axi_arprot;
	sc_signal<sc_bv<4> > m_axi_arqos;
	sc_signal<sc_bv<4> > m_axi_arregion;
	sc_signal<sc_bv<2> > m_axi_aruser;

	// AXI read response channel
	sc_signal<bool> m_axi_rvalid;
	sc_signal<bool> m_axi_rready;
	sc_signal<sc_bv<XLEN*16> > m_axi_rdata;
	sc_signal<bool> m_axi_rlast;
#ifdef DEBUG
	sc_signal<sc_bv<53> > m_axi_rid;
#else
	sc_signal<sc_bv<10> > m_axi_rid;
#endif
	sc_signal<sc_bv<2> > m_axi_rresp;
	sc_signal<sc_bv<2> > m_axi_ruser;

	// DCR write request
	sc_signal<bool> dcr_wr_valid;
	sc_signal<sc_bv<12> > dcr_wr_addr;
	sc_signal<sc_bv<XLEN> > dcr_wr_data;

	// Status
	sc_signal<bool> busy;

	void write_dcr(uint64_t addr, uint64_t value)
	{
		this->dcr_addr = addr;
		this->dcr_data = value;
		ev_write_dcr.notify();
		cout << "function: write_dcr" << endl;
	}

	void attach_ram(RAM* mem)
	{
		ram_ = mem;
	}

	sc_event ev_write_dcr;
	void write_dcr_(void)
	{
		tlm::tlm_generic_payload tr;
		sc_time delay(SC_ZERO_TIME);
		int resp;

		while (1) {
			wait(ev_write_dcr);

			tr.set_command(tlm::TLM_WRITE_COMMAND);
			tr.set_address(dcr_addr);
			tr.set_data_ptr((unsigned char *)&dcr_data);
			tr.set_data_length(sizeof(dcr_data));
			tr.set_dmi_allowed(false);
			tr.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

			dcr_interface->b_transport(tr, delay);

			switch (tr.get_response_status()) {
			case tlm::TLM_OK_RESPONSE:
				resp = 0;
				break;
			case tlm::TLM_ADDRESS_ERROR_RESPONSE:
				resp = -1;
				break;
			default:
				resp = -2;
				break;
			}

			cout << "resp = " << resp << endl;
		}
	}

	Processor(sc_module_name name) :
		axi2tlm("axi2tlm"),
		tlm2dcr("tlm2dcr"),
		rst("rst"),
		reset_n("reset_n"),
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
		m_axi_awuser("m_axi_awuser"),
		m_axi_wvalid("m_axi_wvalid"),
		m_axi_wready("m_axi_wready"),
		m_axi_wdata("m_axi_wdata"),
		m_axi_wstrb("m_axi_wstrb"),
		m_axi_wlast("m_axi_wlast"),
		m_axi_wuser("m_axi_wuser"),
		m_axi_bvalid("m_axi_bvalid"),
		m_axi_bready("m_axi_bready"),
		m_axi_bid("m_axi_bid"),
		m_axi_bresp("m_axi_bresp"),
		m_axi_buser("m_axi_buser"),
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
		m_axi_aruser("m_axi_aruser"),
		m_axi_rvalid("m_axi_rvalid"),
		m_axi_rready("m_axi_rready"),
		m_axi_rdata("m_axi_rdata"),
		m_axi_rlast("m_axi_rlast"),
		m_axi_rid("m_axi_rid"),
		m_axi_rresp("m_axi_rresp"),
		m_axi_ruser("m_axi_ruser"),
		dcr_wr_valid("dcr_wr_valid"),
		dcr_wr_addr("dcr_wr_addr"),
		dcr_wr_data("dcr_wr_data"),
		busy("busy")
	{
		SC_THREAD(write_dcr_);

		ram_ = nullptr;

		/* Slow clock to keep simulation fast.  */
		clk = new sc_clock("clk", sc_time(2, SC_PS));
		device = new VVortex_axi("gpgpu");

		device->clk(*clk);
		device->reset(rst);
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
		axi2tlm.resetn(reset_n);
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
		axi2tlm.awuser(m_axi_awuser);

		axi2tlm.wvalid(m_axi_wvalid);
		axi2tlm.wready(m_axi_wready);
		axi2tlm.wdata(m_axi_wdata);
		axi2tlm.wstrb(m_axi_wstrb);
		axi2tlm.wlast(m_axi_wlast);
		axi2tlm.wuser(m_axi_wuser);

		axi2tlm.bvalid(m_axi_bvalid);
		axi2tlm.bready(m_axi_bready);
		axi2tlm.bid(m_axi_bid);
		axi2tlm.bresp(m_axi_bresp);
		axi2tlm.buser(m_axi_buser);

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
		axi2tlm.aruser(m_axi_aruser);

		axi2tlm.rvalid(m_axi_rvalid);
		axi2tlm.rready(m_axi_rready);
		axi2tlm.rdata(m_axi_rdata);
		axi2tlm.rlast(m_axi_rlast);
		axi2tlm.rid(m_axi_rid);
		axi2tlm.rresp(m_axi_rresp);
		axi2tlm.ruser(m_axi_ruser);

		tlm2dcr.clk(*clk);
		tlm2dcr.resetn(reset_n);
		tlm2dcr.dcr_wr_valid(dcr_wr_valid);
		tlm2dcr.dcr_wr_addr(dcr_wr_addr);
		tlm2dcr.dcr_wr_data(dcr_wr_data);

		dcr_interface.bind(tlm2dcr.tgt_socket);
		axi2tlm.socket.bind(m_axi);

		m_axi.register_b_transport(this, &Processor::b_transport);
	}

private:

	virtual void b_transport(tlm::tlm_generic_payload& trans,
					sc_time& delay)
	{
		tlm::tlm_command cmd = trans.get_command();
		sc_dt::uint64    addr = trans.get_address();
		uint8_t*         data = trans.get_data_ptr();
		unsigned int     len = trans.get_data_length();
		uint8_t*   byteen = trans.get_byte_enable_ptr();
		unsigned int     bye_len = trans.get_byte_enable_length();
		// unsigned int     wid = trans.get_streaming_width();

		//cout << "#################### 0x" << hex <<bye_len << endl;

		if (cmd == tlm::TLM_READ_COMMAND) {
			ram_->read(data, addr, 64);
		} else {
			if (cmd != tlm::TLM_WRITE_COMMAND) {
				trans.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE);
				return;
			}

			cout << "################### " << cmd << " 0x" << std::hex << addr << " 0x" << std::hex << len << endl;
			// check console output
			if (addr >= uint64_t(IO_COUT_ADDR)
				&& addr < (uint64_t(IO_COUT_ADDR) + IO_COUT_SIZE)) {
				for (int i = 0; i < 64; i++) {
					if (int(byteen[i]) & 0xFF) {
						auto& ss_buf = print_bufs_[i];
						char c = int(data[i]);
						ss_buf << c;
						if (c == '\n') {
							std::cout << std::dec << "#" << i << ": " << ss_buf.str() << std::flush;
										ss_buf.str("");
						}
					}
				}
			} else {
				for (int i = 0; i < 64; i++) {
					//cout << "byteen[" << i <<"] = 0x" << hex <<int(byteen[i]) << " data[" << i << "] = 0x" << hex<<int(data[i]) << endl;
					if (int(byteen[i]) & 0xFF) {
						//cout << "addr = 0x" << addr + i << endl;
						(*ram_)[addr + i] = int(data[i]);
					}
				}
			}
		}

		trans.set_response_status(tlm::TLM_OK_RESPONSE);
	}
};

static void show_usage() {
	std::cout << "Usage: [-h: help] <program>" << std::endl;
}

const char* program = nullptr;

static void parse_args(int argc, char **argv) {
	int c;
	while ((c = getopt(argc, argv, "h?")) != -1) {
		switch (c) {
		case 'h':
		case '?':
			show_usage();
			exit(0);
			break;
		default:
			show_usage();
			exit(-1);
		}
	}

	if (optind < argc) {
		program = argv[optind];
		std::cout << "Running " << program << "..." << std::endl;
	} else {
		show_usage();
		exit(-1);
	}
}

int sc_main(int argc, char **argv)
{
	Processor *vortex;
	sc_trace_file *trace_fp = NULL;

	parse_args(argc, argv);

	Verilated::commandArgs(argc, argv);
	sc_set_time_resolution(1, SC_PS);

	trace_fp = sc_create_vcd_trace_file("trace");

	// force random values for unitialized signals
	Verilated::randReset(2);
	Verilated::randSeed(50);
	// turn off assertion before reset
	Verilated::assertOn(false);

	// create memory module
	vortex::RAM ram(4096);

	vortex = new Processor("vortex");

	// attach memory module
	vortex->attach_ram(&ram);
	trace(trace_fp, *vortex, vortex->name());

	cout << "start vortex" << endl;

	/* Pull the reset signal.  */
	vortex->reset_n.write(false);
	sc_start(2, SC_PS);
	vortex->reset_n.write(true);

	// AXI bus reset
	// vortex->m_axi_wready.write(false);
	// vortex->m_axi_awready.write(false);
	// vortex->m_axi_arready.write(false);
	// vortex->m_axi_rvalid.write(false);
	// vortex->m_axi_bvalid.write(false);

	// vortex->dcr_wr_valid.write(false);

	vortex->rst.write(true);
	sc_start(8, SC_PS);
	vortex->rst.write(false);

	// Turn on assertion after reset
	Verilated::assertOn(true);

	vortex->write_dcr(0x1, 0x80000000);
	sc_start(10, sc_core::SC_PS);
	vortex->write_dcr(0x3, 0x00000000);

	// load program
	{
		std::string program_ext(fileExtension(program));
		if (program_ext == "bin") {
			ram.loadBinImage(program, 0x80000000);
		} else if (program_ext == "hex") {
			ram.loadHexImage(program);
		} else {
			std::cout << "*** error: only *.bin or *.hex images supported." << std::endl;
			return -1;
		}
	}

	sc_start();
	if (trace_fp) {
		sc_close_vcd_trace_file(trace_fp);
	}

	return 0;
}