/*
 * This file is part of the coreboot project.
 *
 * Copyright (C) 2014 Google Inc.
 * Copyright (C) 2015 Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <arch/acpi.h>
#include <arch/io.h>
#include <cbmem.h>
#include <console/console.h>
#include <device/device.h>
#include <device/pci.h>
#include <device/pci_def.h>
#include <reg_script.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <soc/iomap.h>
#include <soc/smbus.h>
#include <soc/pci_devs.h>
#include <soc/pm.h>
#include <soc/pmc.h>
#include <soc/romstage.h>
#include <vboot/vboot_common.h>
#include <intelblocks/pmclib.h>

/* Return 0, 3, or 5 to indicate the previous sleep state. */
int soc_prev_sleep_state(const struct chipset_power_state *ps,
						int prev_sleep_state)
{

	/*
	 * Check for any power failure to determine if this a wake from
	 * S5 because the PCH does not set the WAK_STS bit when waking
	 * from a true G3 state.
	 */
	if (!(ps->pm1_sts & WAK_STS) &&
	    (ps->gen_pmcon_b & (PWR_FLR | SUS_PWR_FLR)))
		prev_sleep_state = ACPI_S5;

	/*
	 * If waking from S3 determine if deep S3 is enabled. If not,
	 * need to check both deep sleep well and normal suspend well.
	 * Otherwise just check deep sleep well.
	 */
	if (prev_sleep_state == ACPI_S3) {
		/* PWR_FLR represents deep sleep power well loss. */
		uint32_t mask = PWR_FLR;

		/* If deep s3 isn't enabled check the suspend well too. */
		if (!deep_s3_enabled())
			mask |= SUS_PWR_FLR;

		if (ps->gen_pmcon_b & mask)
			prev_sleep_state = ACPI_S5;
	}
	return prev_sleep_state;
}

void soc_fill_power_state(struct chipset_power_state *ps)
{
	uint16_t tcobase;
	uint8_t *pmc;

	tcobase = smbus_tco_regs();

	ps->tco1_sts = inw(tcobase + TCO1_STS);
	ps->tco2_sts = inw(tcobase + TCO2_STS);

	printk(BIOS_DEBUG, "TCO_STS:   %04x %04x\n",
	       ps->tco1_sts, ps->tco2_sts);

	ps->gen_pmcon_a = pci_read_config32(PCH_DEV_PMC, GEN_PMCON_A);
	ps->gen_pmcon_b = pci_read_config32(PCH_DEV_PMC, GEN_PMCON_B);

	pmc = pmc_mmio_regs();
	ps->gblrst_cause[0] = read32(pmc + GBLRST_CAUSE0);
	ps->gblrst_cause[1] = read32(pmc + GBLRST_CAUSE1);

	printk(BIOS_DEBUG, "GEN_PMCON: %08x %08x\n",
	       ps->gen_pmcon_a, ps->gen_pmcon_b);

	printk(BIOS_DEBUG, "GBLRST_CAUSE: %08x %08x\n",
	       ps->gblrst_cause[0], ps->gblrst_cause[1]);
}

int acpi_get_sleep_type(void)
{
	struct chipset_power_state *ps;

	ps = pmc_get_power_state();
	return ps->prev_sleep_state;
}
