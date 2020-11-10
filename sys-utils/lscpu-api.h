#ifndef LSCPU_API_H
#define LSCPU_API_H

#include "c.h"
#include "nls.h"
#include "cpuset.h"
#include "xalloc.h"
#include "strutils.h"
#include "bitops.h"
#include "path.h"
#include "pathnames.h"
#include "all-io.h"
#include "debug.h"

#define LSCPU_DEBUG_INIT	(1 << 1)
#define LSCPU_DEBUG_MISC	(1 << 2)
#define LSCPU_DEBUG_GATHER	(1 << 3)
#define LSCPU_DEBUG_TYPE	(1 << 4)
#define LSCPU_DEBUG_CPU		(1 << 5)
#define LSBLK_DEBUG_ALL		0xFFFF

UL_DEBUG_DECLARE_MASK(lscpu);
#define DBG(m, x)       __UL_DBG(lscpu, LSCPU_DEBUG_, m, x)
#define ON_DBG(m, x)    __UL_DBG_CALL(lscpu, LSCPU_DEBUG_, m, x)

#define UL_DEBUG_CURRENT_MASK	UL_DEBUG_MASK(lscpu)
#include "debugobj.h"

#define _PATH_SYS_SYSTEM	"/sys/devices/system"
#define _PATH_SYS_HYP_FEATURES	"/sys/hypervisor/properties/features"
#define _PATH_SYS_CPU		_PATH_SYS_SYSTEM "/cpu"
#define _PATH_SYS_NODE		_PATH_SYS_SYSTEM "/node"

struct lscpu_cputype {
	int	refcount;

	char	*arch;
	char	*vendor;
	char	*machinetype;	/* s390 */
	char	*family;
	char	*model;
	char	*modelname;
	char	*revision;	/* alternative for model (ppc) */
	char	*virtflag;	/* virtualization flag (vmx, svm) */
	char	*hypervisor;	/* hypervisor software */
	int	hyper;		/* hypervisor vendor ID */
	int	virtype;	/* VIRT_PARA|FULL|NONE ? */
	char	*stepping;
	char    *bogomips;
	char	*flags;
	char	*mtid;		/* maximum thread id (s390) */
	char	*addrsz;	/* address sizes */
	int	dispatching;	/* none, horizontal or vertical */
	int	freqboost;	/* -1 if not evailable */

	int	*polarization;	/* cpu polarization */
	int	*addresses;	/* physical cpu addresses */
	int	*configured;	/* cpu configured */
	int	physsockets;	/* Physical sockets (modules) */
	int	physchips;	/* Physical chips */
	int	physcoresperchip;	/* Physical cores per chip */

	int	ncores;
	int	nbooks;
	int	threads;
	int	ndrawers;

	unsigned int	bit32:1,
			bit64:1;
};

struct lscpu_cpu {
	int refcount;
	struct lscpu_cputype *type;

	int logical_id;
	char	*mhz;

	char	*dynamic_mhz;
	char	*static_mhz;
};

struct lscpu_cxt {
	const char *prefix;	 /* path to /sys and /proc snapshot or NULL */

	struct path_cxt	*syscpu; /* _PATH_SYS_CPU path handler */
	struct path_cxt *procfs; /* /proc path handler */

	size_t ncputypes;
	struct lscpu_cputype **cputypes;

	size_t ncpus;
	struct lscpu_cpu **cpus;

};

struct lscpu_cputype *lscpu_new_cputype(void);
void lscpu_ref_cputype(struct lscpu_cputype *ct);
void lscpu_unref_cputype(struct lscpu_cputype *ct);
struct lscpu_cputype *lscpu_add_cputype(struct lscpu_cxt *cxt, struct lscpu_cputype *ct);
struct lscpu_cputype *lscpu_cputype_get_default(struct lscpu_cxt *cxt);
int lscpu_read_cpuinfo(struct lscpu_cxt *cxt);

struct lscpu_cpu *lscpu_new_cpu(void);
void lscpu_ref_cpu(struct lscpu_cpu *cpu);
void lscpu_unref_cpu(struct lscpu_cpu *cpu);
int lscpu_add_cpu(struct lscpu_cxt *cxt,
                  struct lscpu_cpu *cpu,
                  struct lscpu_cputype *ct);
int lscpu_cpus_apply_type(struct lscpu_cxt *cxt, struct lscpu_cputype *type);

struct lscpu_cxt *lscpu_new_context(void);
void lscpu_free_context(struct lscpu_cxt *cxt);

#endif /* LSCPU_API_H */
