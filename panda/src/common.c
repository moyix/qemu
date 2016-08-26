#include <assert.h>
#include <stdint.h>


#include "qemu/osdep.h"
#include "cpu.h"
#include "disas/disas.h"
#include "../include/panda/plugin.h"
#include "../include/panda/common.h"
#include "../include/panda/plog.h"

target_ulong panda_current_pc(CPUState *cpu) {
    target_ulong pc, cs_base;
    uint32_t flags;
    CPUArchState *env = cpu->env_ptr;  
    cpu_get_tb_cpu_state(env, &pc, &cs_base, &flags);
    return pc;
}

#ifdef TARGET_ARM
/* Return the exception level which controls this address translation regime */
static inline uint32_t regime_el(CPUARMState *env, ARMMMUIdx mmu_idx)
{
    switch (mmu_idx) {
    case ARMMMUIdx_S2NS:
    case ARMMMUIdx_S1E2:
        return 2;
    case ARMMMUIdx_S1E3:
        return 3;
    case ARMMMUIdx_S1SE0:
        return arm_el_is_aa64(env, 3) ? 1 : 3;
    case ARMMMUIdx_S1SE1:
    case ARMMMUIdx_S1NSE0:
    case ARMMMUIdx_S1NSE1:
        return 1;
    default:
        g_assert_not_reached();
    }
}

/* Return the TCR controlling this translation regime */
static inline TCR *regime_tcr(CPUARMState *env, ARMMMUIdx mmu_idx)
{
    if (mmu_idx == ARMMMUIdx_S2NS) {
        return &env->cp15.vtcr_el2;
    }
    return &env->cp15.tcr_el[regime_el(env, mmu_idx)];
}

/* Return the TTBR associated with this translation regime */
static inline uint64_t regime_ttbr(CPUARMState *env, ARMMMUIdx mmu_idx,
                                   int ttbrn)
{
    if (mmu_idx == ARMMMUIdx_S2NS) {
        return env->cp15.vttbr_el2;
    }
    if (ttbrn == 0) {
        return env->cp15.ttbr0_el[regime_el(env, mmu_idx)];
    } else {
        return env->cp15.ttbr1_el[regime_el(env, mmu_idx)];
    }
}
// ARM: stolen get_level1_table_address ()
// from target-arm/helper.c
bool arm_get_vaddr_table(CPUState *cpu, uint32_t *table, uint32_t address);
bool arm_get_vaddr_table(CPUState *cpu, uint32_t *table, uint32_t address)
{
    CPUARMState *env = (CPUARMState *)cpu->env_ptr;
    ARMMMUIdx mmu_idx = cpu_mmu_index(env, false);
    /* Note that we can only get here for an AArch32 PL0/PL1 lookup */
    TCR *tcr = regime_tcr(env, mmu_idx);

    if (address & tcr->mask) {
        if (tcr->raw_tcr & TTBCR_PD1) {
            /* Translation table walk disabled for TTBR1 */
            return false;
        }
        *table = regime_ttbr(env, mmu_idx, 1) & 0xffffc000;
    } else {
        if (tcr->raw_tcr & TTBCR_PD0) {
            /* Translation table walk disabled for TTBR0 */
            return false;
        }
        *table = regime_ttbr(env, mmu_idx, 0) & tcr->base_mask;
    }
    *table |= (address >> 18) & 0x3ffc;
    return true;
}
/*static uint32_t arm_get_vaddr_table(CPUState *cpu, uint32_t address)*/
/*{*/
  /*uint32_t table;*/

  /*if (address & env->cp15.c2_mask)*/
    /*table = env->cp15.c2_base1 & 0xffffc000;*/
  /*else*/
    /*table = env->cp15.c2_base0 & env->cp15.c2_base_mask;*/

  /*return table;*/
/*}*/
#endif

/*
  returns current asid or address-space id.
  architecture-independent
*/
target_ulong panda_current_asid(CPUState *cpu) {
#if (defined TARGET_I386 || defined TARGET_X86_64)
  CPUArchState *env = cpu->env_ptr;
  return env->cr[3];
#elif defined(TARGET_ARM)
  target_ulong table;
  bool rc = arm_get_vaddr_table(cpu,
          &table,
          panda_current_pc(cpu));
  assert(rc);
  return table;
  /*return arm_get_vaddr_table(env, panda_current_pc(env));*/
#else
#error "panda_current_asid() not implemented for target architecture."
  return 0;
#endif
}

/*
  returns true if we are currently executing in kernel-mode
*/

bool panda_in_kernel(CPUState *cpu) {
   CPUArchState *env = cpu->env_ptr;
#if defined(TARGET_I386)
    return ((env->hflags & HF_CPL_MASK) == 0);
#elif defined(TARGET_ARM)
    return ((env->uncached_cpsr & CPSR_M) == ARM_CPU_MODE_SVC);
#else
#error "panda_in_kernel() not implemented for target architecture."
    return false;
#endif
}


void panda_disas(FILE *out, void *code, unsigned long size) {
    disas(out,code,size);
}

const char * valid_os[] = {
    "windows-32-xpsp2", 
    "windows-32-xpsp3", 
    "windows-32-7", 
    "linux-32-lava32",
    "linux-64-3.2.63",
    "linux-32-3.2.65",
    "linux-32-3.2.54",
    NULL
};



PandaOsType panda_os_type = OST_UNKNOWN;
char *panda_os_name = NULL;
uint32_t panda_os_bits = 0;  // 32 or 64
char *panda_os_details = NULL;

void panda_set_os_name(char *os_name) {
    int i=0;
    bool ok_osname = false;
    while (valid_os[i]) {
        if (0 == strcmp(os_name, valid_os[i])) {
            ok_osname = true;
            break;
        }
        i++;
    }
    if (!ok_osname) {
        i=0;
        printf ("os_name=[%s] is not on the list :\n", os_name);
        while (valid_os[i]) {
            printf ("  [%s]\n",  valid_os[i]);
            i++;
        }
        assert (ok_osname);
    }
    panda_os_name = strdup(os_name);
    panda_os_type = OST_UNKNOWN;
    char *p = os_name;
    if (0 == strncmp("windows", os_name, 7))  {
        panda_os_type = OST_WINDOWS;
        p += 8;
    }
    if (0 == strncmp("linux", os_name, 5))  {
        panda_os_type = OST_LINUX;
        p += 6;
    }
    assert (!(panda_os_type == OST_UNKNOWN));
    printf ("p= %s\n", p);
    if (0 == strncmp("32", p, 2)) {
        panda_os_bits = 32;
    }
    if (0 == strncmp("64", p, 2)) {
        panda_os_bits = 64;
    }
    assert (panda_os_bits != 0);
    p += 3;
    panda_os_details = strdup(p);
    printf ("os_type=%d bits=%d os_details=[%s]\n", 
            panda_os_type, panda_os_bits, panda_os_details); 
}


// Note: these call things in exec.c
void panda_invalidate_single_tb(CPUState *cpu, target_ulong pc) {
    breakpoint_invalidate(cpu, pc);
}

int panda_physical_memory_rw(hwaddr addr, uint8_t *buf, int len, int is_write) {
    return cpu_physical_memory_rw_ex(addr, buf, len, is_write, true);
}


hwaddr panda_virt_to_phys(CPUState *env, target_ulong addr){
    target_ulong page;
    hwaddr phys_addr;
    page = addr & TARGET_PAGE_MASK;
    phys_addr = cpu_get_phys_page_debug(env, page);
    /* if no physical page mapped, return an error */
    if (phys_addr == -1)
        return -1;
    phys_addr += (addr & ~TARGET_PAGE_MASK);
    return phys_addr;
}

int panda_virtual_memory_rw(CPUState *env, target_ulong addr,
                        uint8_t *buf, int len, int is_write)
{
    int l;
    int ret;
    hwaddr phys_addr;
    target_ulong page;

    while (len > 0) {
        page = addr & TARGET_PAGE_MASK;
        phys_addr = cpu_get_phys_page_debug(env, page);
        /* if no physical page mapped, return an error */
        if (phys_addr == -1)
            return -1;
        l = (page + TARGET_PAGE_SIZE) - addr;
        if (l > len)
            l = len;
        phys_addr += (addr & ~TARGET_PAGE_MASK);
        #warning TRL fixme if necessary
        assert (!is_write);
//        if (is_write)
//            cpu_physical_memory_write_rom(phys_addr, buf, l);
//        else {
            ret = panda_physical_memory_rw(phys_addr, buf, l, is_write);
            if(ret < 0) return ret;
//        }
        len -= l;
        buf += l;
        addr += l;
    }
    return 0;
}


int panda_virtual_memory_read(CPUState *env, target_ulong addr,
                              uint8_t *buf, int len) {
    return panda_virtual_memory_rw(env, addr, buf, len, 0);
}


int panda_virtual_memory_write(CPUState *env, target_ulong addr,
                               uint8_t *buf, int len) {
    return panda_virtual_memory_rw(env, addr, buf, len, 1);
}


void panda_cleanup(void) {
    // PANDA: unload plugins
    panda_unload_plugins();
    if (pandalog) {
        pandalog_close();
    }
}


