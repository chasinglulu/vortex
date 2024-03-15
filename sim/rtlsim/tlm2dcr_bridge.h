/*
 * TLM-2.0 to Vortex DCR bridge.
 *
 * Copyright (C) 2024 Charleye <wangkart@aliyun.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#ifndef TLM2DCR_BRIDGE_H__
#define TLM2DCR_BRIDGE_H__
#define SC_INCLUDE_DYNAMIC_PROCESSES

#include "tlm-extensions/genattr.h"

template <int DATA_WIDTH>
class tlm2dcr_bridge
: public sc_core::sc_module
{
public:
	tlm_utils::simple_target_socket<tlm2dcr_bridge> tgt_socket;

	SC_HAS_PROCESS(tlm2dcr_bridge);

	sc_in<bool> clk;
	sc_in<bool> resetn;

	sc_out<bool> dcr_wr_valid;
	sc_out<sc_bv<12> > dcr_wr_addr;
	sc_out<sc_bv<DATA_WIDTH> > dcr_wr_data;

	sc_mutex m_mutex;

	tlm2dcr_bridge(sc_core::sc_module_name name) :
		sc_module(name),
		tgt_socket("tgt_socket"),

		clk("clk"),
		resetn("resetn"),
		dcr_wr_valid("dcr_wr_valid"),
		dcr_wr_addr("dcr_wr_addr"),
		dcr_wr_data("dcr_wr_data")
	{
		tgt_socket.register_b_transport(this, &tlm2dcr_bridge::b_transport);
	}

	bool reset_asserted()
	{
		return resetn.read() == false;
	}

private:


	virtual void b_transport(tlm::tlm_generic_payload& trans,
					sc_time& delay)
	{
		tlm::tlm_command cmd = trans.get_command();
		sc_dt::uint64 addr = trans.get_address();
		uint8_t *data = trans.get_data_ptr();
		unsigned int len = trans.get_data_length();

		// Since we're going to do waits in order to wiggle the
		// AXI signals, we need to eliminate the accumulated
		// TLM delay.
		wait(delay, resetn.negedge_event());
		delay = SC_ZERO_TIME;

		m_mutex.lock();

		wait(clk.posedge_event());

		/* Abort transaction if reset is asserted. */
		if (reset_asserted()) {
			trans.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
			m_mutex.unlock();
			return;
		}
		dcr_wr_valid.write(true);
		dcr_wr_addr.write(addr);
		dcr_wr_data.write(*((uint32_t *)data));

		do {
			wait(clk.negedge_event());

			dcr_wr_valid.write(false);
		} while (dcr_wr_valid.read() == true);

		trans.set_response_status(tlm::TLM_OK_RESPONSE);

		m_mutex.unlock();
	}
};

#endif
