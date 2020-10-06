/* Copyright 2019 SiFive, Inc */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdlib.h>
#include <stdio.h>

#include <metal/cpu.h>
#include <metal/interrupt_handlers.h>
#include <metal/pmp.h>
#include <metal/riscv.h>

#define NAPOT_SIZE 128
#define PROTECTED_ARRAY_LENGTH 32
volatile uint32_t protected_global[PROTECTED_ARRAY_LENGTH] __attribute__((aligned(NAPOT_SIZE)));

void metal_exception_store_amo_access_fault_handler(struct metal_cpu cpu, int ecode)
{
	/* Get faulting instruction and instruction length */
	unsigned long epc = metal_cpu_get_exception_pc(cpu);
	int inst_len = metal_cpu_get_instruction_length(cpu, epc);

	/* Advance the exception program counter by the length of the
	 * instruction to return execution after the faulting store */
	metal_cpu_set_exception_pc(cpu, epc + inst_len);
}

int main()
{
	/* PMP addresses are 4-byte aligned, drop the bottom two bits */
	uintptr_t protected_addr = ((uintptr_t) &protected_global) >> 2;
	
	/* Clear the bit corresponding with alignment */
	protected_addr &= ~(NAPOT_SIZE >> 3);

	/* Set the bits up to the alignment bit */
	protected_addr |= ((NAPOT_SIZE >> 3) - 1);

	printf("PMP Driver Example\n");

	/* Initialize PMPs */
	metal_pmp_init();

	/* Perform a write to the memory we're about to protect access to */
	protected_global[0] = 0;

	/* Configure PMP 0 to only allow reads to protected_global. The
	 * PMP region is locked so that the configuration applies to M-mode
	 * accesses. */
	struct metal_pmp_config config = {
		.L = METAL_PMP_LOCKED,
		.A = METAL_PMP_NAPOT, /* Naturally-aligned power of two */
		.X = 0,
		.W = 0,
		.R = 1,
	};
	if (metal_pmp_set_region(0, config, protected_addr) != 0) {
		printf("Failed to configure PMP 0\n");
		return 5;
	}

	printf("Attempting to write to protected address\n");

	/* Attempt to write to protected_global. This should generate a store
	 * access fault exception. */
	protected_global[0] = 6;

	if (protected_global[0] == 0) {
		printf("PMP protection succeeeded.\n");
	} else {
		printf("PMP protection failed.\n");
	}

	/* If the write succeeds, the test fails */
	return protected_global[0];
}
