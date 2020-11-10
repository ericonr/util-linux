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
#define LSCPU_DEBUG_VIRT	(1 << 6)
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

	char	*vendor;
	char	*machinetype;	/* s390 */
	char	*family;
	char	*model;
	char	*modelname;
	char	*revision;	/* alternative for model (ppc) */
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

	int	ncpus;		/* how many CPUs references this type */
	int	nthreads;	/* calculated (probably same as ncpus) */

	int		ncores;
	cpu_set_t	**coremaps;
	int		nsockets;
	cpu_set_t       **socketmaps;
	int		nbooks;
	cpu_set_t	**bookmaps;
	int		ndrawers;
	cpu_set_t	**drawermaps;
};

struct lscpu_cpu {
	int refcount;
	struct lscpu_cputype *type;

	int logical_id;
	char	*mhz;

	char	*dynamic_mhz;
	char	*static_mhz;

	int	coreid;
	int	socketid;
	int	bookid;
	int	drawerid;
};

struct lscpu_arch {
	char	*name;		/* uname() .machine */

	unsigned int	bit32:1,
			bit64:1;
};

struct lscpu_vulnerability {
	char	*name;
	char	*text;
};

/* virtualization types */
enum {
	VIRT_TYPE_NONE	= 0,
	VIRT_TYPE_PARA,
	VIRT_TYPE_FULL,
	VIRT_TYPE_CONTAINER
};

/* hypervisor vendors */
enum {
	VIRT_VENDOR_NONE	= 0,
	VIRT_VENDOR_XEN,
	VIRT_VENDOR_KVM,
	VIRT_VENDOR_MSHV,
	VIRT_VENDOR_VMWARE,
	VIRT_VENDOR_IBM,		/* sys-z powervm */
	VIRT_VENDOR_VSERVER,
	VIRT_VENDOR_UML,
	VIRT_VENDOR_INNOTEK,		/* VBOX */
	VIRT_VENDOR_HITACHI,
	VIRT_VENDOR_PARALLELS,	/* OpenVZ/VIrtuozzo */
	VIRT_VENDOR_VBOX,
	VIRT_VENDOR_OS400,
	VIRT_VENDOR_PHYP,
	VIRT_VENDOR_SPAR,
	VIRT_VENDOR_WSL,
};

struct lscpu_virt {
	char	*cpuflag;	/* virtualization flag (vmx, svm) */
	char	*hypervisor;	/* hypervisor software */
	int	vendor;		/* VIRT_VENDOR_* */
	int	type;		/* VIRT_TYPE_* ? */

};

struct lscpu_cxt {
	int maxcpus;		/* size in bits of kernel cpu mask */
	const char *prefix;	/* path to /sys and /proc snapshot or NULL */

	struct path_cxt	*syscpu; /* _PATH_SYS_CPU path handler */
	struct path_cxt *procfs; /* /proc path handler */

	size_t ncputypes;
	struct lscpu_cputype **cputypes;

	/* CPUs as read from /proc/cpuinfo, it means online CPUs only */
	size_t ncpus;
	struct lscpu_cpu **cpus;

	/*
	 * All maps are sequentially indexed (0..ncpuspos), the array index
	 * does not have match with cpuX number as presented by kernel. You
	 * have to use real_cpu_num() to get the real cpuX number.
	 *
	 * For example, the possible system CPUs are: 1,3,5, it means that
	 * ncpuspos=3, so all arrays are in range 0..3.
	 *
	 * TODO: Do we really need it if we have lscpu_cpu->logical_id?
	 */
	size_t ncpuspos;	/* maximal possible CPUs */
	int *idx2cpunum;	/* mapping index to CPU num */

	size_t npresents;
	cpu_set_t *present;	/* mask with present CPUs */

	size_t nonlines;	/* aka number of trhreads */
	cpu_set_t *online;	/* mask with online CPUs */

	struct lscpu_arch *arch;
	struct lscpu_virt *virt;

	struct lscpu_vulnerability *vuls;	/* array of CPU vulnerabilities */
	size_t  nvuls;				/* number of CPU vulnerabilities */

	size_t nnodes;		/* number of NUMA modes */
	int *idx2nodenum;	/* Support for discontinuous nodes */
	cpu_set_t **nodemaps;	/* array with NUMA nodes */

	unsigned int noalive : 1;
};

struct lscpu_cputype *lscpu_new_cputype(void);
void lscpu_ref_cputype(struct lscpu_cputype *ct);
void lscpu_unref_cputype(struct lscpu_cputype *ct);
struct lscpu_cputype *lscpu_add_cputype(struct lscpu_cxt *cxt, struct lscpu_cputype *ct);
struct lscpu_cputype *lscpu_cputype_get_default(struct lscpu_cxt *cxt);

int lscpu_read_cpuinfo(struct lscpu_cxt *cxt);
int lscpu_read_cpulists(struct lscpu_cxt *cxt);
int lscpu_read_archext(struct lscpu_cxt *cxt);
int lscpu_read_vulnerabilities(struct lscpu_cxt *cxt);
int lscpu_read_numas(struct lscpu_cxt *cxt);
int lscpu_read_topology(struct lscpu_cxt *cxt);

struct lscpu_arch *lscpu_read_architecture(struct lscpu_cxt *cxt);
void lscpu_free_architecture(struct lscpu_arch *ar);

struct lscpu_virt *lscpu_read_virtualization(struct lscpu_cxt *cxt);
void lscpu_free_virtualization(struct lscpu_virt *virt);

struct lscpu_cpu *lscpu_new_cpu(void);
void lscpu_ref_cpu(struct lscpu_cpu *cpu);
void lscpu_unref_cpu(struct lscpu_cpu *cpu);
int lscpu_add_cpu(struct lscpu_cxt *cxt,
                  struct lscpu_cpu *cpu,
                  struct lscpu_cputype *ct);
int lscpu_cpus_apply_type(struct lscpu_cxt *cxt, struct lscpu_cputype *type);
struct lscpu_cpu *lscpu_cpus_loopup_by_type(struct lscpu_cxt *cxt, struct lscpu_cputype *ct);

void lscpu_decode_arm(struct lscpu_cxt *cxt);

struct lscpu_cxt *lscpu_new_context(void);
void lscpu_free_context(struct lscpu_cxt *cxt);

int lookup(char *line, char *pattern, char **value);

/*
 * Firmware stuff
 */
#define _PATH_SYS_DMI_TYPE4     "/sys/firmware/dmi/entries/4-0/raw"
#define _PATH_SYS_DMI		"/sys/firmware/dmi/tables/DMI"

struct lscpu_dmi_header
{
	uint8_t type;
	uint8_t length;
	uint16_t handle;
	uint8_t *data;
};

static inline void to_dmi_header(struct lscpu_dmi_header *h, uint8_t *data)
{
	h->type = data[0];
	h->length = data[1];
	memcpy(&h->handle, data + 2, sizeof(h->handle));
	h->data = data;
}

static inline char *dmi_string(const struct lscpu_dmi_header *dm, uint8_t s)
{
	char *bp = (char *)dm->data;

	if (!s || !bp)
		return NULL;

	bp += dm->length;
	while (s > 1 && *bp) {
		bp += strlen(bp);
		bp++;
		s--;
	}

	return !*bp ? NULL : bp;
}

#endif /* LSCPU_API_H */
