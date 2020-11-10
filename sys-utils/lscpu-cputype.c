
#include "lscpu-api.h"

UL_DEBUG_DEFINE_MASK(lscpu);
UL_DEBUG_DEFINE_MASKNAMES(lscpu) = UL_DEBUG_EMPTY_MASKNAMES;

static void lscpu_init_debug(void)
{
	__UL_INIT_DEBUG_FROM_ENV(lscpu, LSCPU_DEBUG_, 0, LSCPU_DEBUG);
}

static void context_init_paths(struct lscpu_cxt *cxt)
{
	DBG(MISC, ul_debugobj(cxt, "initialize paths"));
	ul_path_init_debug();

	/* /sys/devices/system/cpu */
	cxt->syscpu = ul_new_path(_PATH_SYS_CPU);
	if (!cxt->syscpu)
		err(EXIT_FAILURE, _("failed to initialize CPUs sysfs handler"));
	if (cxt->prefix)
		ul_path_set_prefix(cxt->syscpu, cxt->prefix);

	/* /proc */
	cxt->procfs = ul_new_path("/proc");
	if (!cxt->procfs)
		err(EXIT_FAILURE, _("failed to initialize procfs handler"));
	if (cxt->prefix)
		ul_path_set_prefix(cxt->procfs, cxt->prefix);
}


struct lscpu_cputype *lscpu_new_cputype(void)
{
	struct lscpu_cputype *ct;

	ct = xcalloc(1, sizeof(struct lscpu_cputype));
	ct->refcount = 1;

	DBG(TYPE, ul_debugobj(ct, "alloc"));
	return ct;
}

void lscpu_ref_cputype(struct lscpu_cputype *ct)
{
	if (ct)
		ct->refcount++;
}

void lscpu_unref_cputype(struct lscpu_cputype *ct)
{
	if (!ct)
		return;

	if (--ct->refcount <= 0) {
		DBG(TYPE, ul_debugobj(ct, "  freeing"));
		free(ct);
	}
}

struct lscpu_cputype *lscpu_cputype_get_default(struct lscpu_cxt *cxt)
{
	return cxt->cputypes ? cxt->cputypes[0] : NULL;
}

#define match(astr, bstr) \
		((!astr && !bstr) || (astr && bstr && strcmp(astr, bstr) == 0))

struct lscpu_cputype *lscpu_add_cputype(struct lscpu_cxt *cxt, struct lscpu_cputype *ct)
{
	size_t i;

	/* ignore if already in the context */
	for (i = 0; i < cxt->ncputypes; i++) {
		struct lscpu_cputype *x = cxt->cputypes[i];

		if (match(x->vendor, ct->vendor) &&
		    match(x->model, ct->model) &&
		    match(x->modelname, ct->modelname) &&
		    match(x->stepping, ct->stepping)) {

			DBG(TYPE, ul_debugobj(x, "reuse"));
			return x;
		}
	}

	DBG(TYPE, ul_debugobj(ct, "add new"));
	cxt->cputypes = xrealloc(cxt->cputypes, (cxt->ncputypes + 1)
				* sizeof(struct lscpu_cputype *));
	cxt->cputypes[cxt->ncputypes] = ct;
	cxt->ncputypes++;
	lscpu_ref_cputype(ct);

	/* first type -- use it for all CPUs */
	if (cxt->ncputypes == 1)
		lscpu_cpus_apply_type(cxt, ct);

	return ct;
}

static void lscpu_merge_cputype(struct lscpu_cputype *a, struct lscpu_cputype *b)
{
	if (!a->arch && b->arch)
		a->arch = xstrdup(b->arch);
	if (!a->vendor && b->vendor)
		a->vendor = xstrdup(b->vendor);
	if (!a->machinetype && b->machinetype)
		a->machinetype = xstrdup(b->machinetype);
	if (!a->family && b->family)
		a->family = xstrdup(b->family);
	if (!a->model && b->model)
		a->model = xstrdup(b->model);
	if (!a->modelname && b->modelname)
		a->modelname = xstrdup(b->modelname);
	if (!a->revision && b->revision)
		a->revision = xstrdup(b->revision);
	if (!a->virtflag && b->virtflag)
		a->virtflag = xstrdup(b->virtflag);
	if (!a->hypervisor && b->hypervisor)
		a->hypervisor  = xstrdup(b->hypervisor);
	if (!a->stepping && b->stepping)
		a->stepping = xstrdup(b->stepping);
	if (!a->bogomips && b->bogomips)
		a->bogomips = xstrdup(b->bogomips);
	if (!a->flags && b->flags)
		a->flags = xstrdup(b->flags);
	if (!a->mtid && b->mtid)
		a->mtid = xstrdup(b->mtid);
	if (!a->addrsz && b->addrsz)
		a->addrsz = xstrdup(b->addrsz);
}

/* Describes /proc/cpuinfo fields */
struct cpuinfo_pattern {
	int id;			/* field ID */
	const char *pattern;	/* field name as used in /proc/cpuinfo */
	size_t	offset;		/* offset in lscpu_cputype or lscpu_cpu struct */
};

/* field identifiers (field name may be different on different archs) */
enum {
	PAT_ADDRESS_SIZES,
	PAT_BOGOMIPS,
	PAT_CPU,
	PAT_FAMILY,
	PAT_FEATURES,
	PAT_FLAGS,
	PAT_IMPLEMENTER,
	PAT_MAX_THREAD_ID,
	PAT_MHZ,
	PAT_MHZ_DYNAMIC,
	PAT_MHZ_STATIC,
	PAT_MODEL,
	PAT_MODEL_NAME,
	PAT_PART,
	PAT_PROCESSOR,
	PAT_REVISION,
	PAT_STEPPING,
	PAT_TYPE,
	PAT_VARIANT,
	PAT_VENDOR,
};

/*
 * /proc/cpuinfo to lscpu_cputype conversion
 */
#define DEF_PAT_CPUTYPE(_str, _id, _member) \
	{ \
		.id = (_id), \
		.pattern = (_str), \
		.offset = offsetof(struct lscpu_cputype, _member), \
	}

static const struct cpuinfo_pattern type_patterns[] =
{
	/* Sort by fields name! */
	DEF_PAT_CPUTYPE( "BogoMIPS",		PAT_BOGOMIPS,	bogomips),	/* aarch64 */
	DEF_PAT_CPUTYPE( "CPU implementer",	PAT_IMPLEMENTER,vendor),	/* ARM and aarch64 */
	DEF_PAT_CPUTYPE( "CPU part",		PAT_PART,	model),		/* ARM and aarch64 */
	DEF_PAT_CPUTYPE( "CPU revision",	PAT_REVISION,	revision),	/* aarch64 */
	DEF_PAT_CPUTYPE( "CPU variant",		PAT_VARIANT,	stepping),	/* aarch64 */
	DEF_PAT_CPUTYPE( "Features",		PAT_FEATURES,	flags),		/* aarch64 */
	DEF_PAT_CPUTYPE( "address sizes",	PAT_ADDRESS_SIZES,	addrsz),/* x86 */
	DEF_PAT_CPUTYPE( "bogomips",		PAT_BOGOMIPS,	bogomips),
	DEF_PAT_CPUTYPE( "bogomips per cpu",	PAT_BOGOMIPS,	bogomips),	/* s390 */
	DEF_PAT_CPUTYPE( "cpu family",		PAT_FAMILY,	family),
	DEF_PAT_CPUTYPE( "cpu",			PAT_CPU,	modelname),	/* ppc, sparc */
	DEF_PAT_CPUTYPE( "family",		PAT_FAMILY,	family),
	DEF_PAT_CPUTYPE( "features",		PAT_FEATURES,	flags),		/* s390 */
	DEF_PAT_CPUTYPE( "flags",		PAT_FLAGS,	flags),		/* x86 */
	DEF_PAT_CPUTYPE( "max thread id",	PAT_MAX_THREAD_ID, mtid),	/* s390 */
	DEF_PAT_CPUTYPE( "model name",		PAT_MODEL_NAME,	modelname),
	DEF_PAT_CPUTYPE( "model",		PAT_MODEL,	model),
	DEF_PAT_CPUTYPE( "revision",		PAT_REVISION,	revision),
	DEF_PAT_CPUTYPE( "stepping",		PAT_STEPPING,	stepping),
	DEF_PAT_CPUTYPE( "type",		PAT_TYPE,	flags),		/* sparc64 */
	DEF_PAT_CPUTYPE( "vendor",		PAT_VENDOR,	vendor),
	DEF_PAT_CPUTYPE( "vendor_id",		PAT_VENDOR,	vendor),	/* s390 */
};

/*
 * /proc/cpuinfo to lscpu_cpu conversion
 */
#define DEF_PAT_CPU(_str, _id, _member) \
	{ \
		.id = (_id), \
		.pattern = (_str), \
		.offset = offsetof(struct lscpu_cpu, _member), \
	}

static const struct cpuinfo_pattern cpu_patterns[] =
{
	/* Sort by fields name! */
	DEF_PAT_CPU( "cpu MHz dynamic",	PAT_MHZ_DYNAMIC,dynamic_mhz),	/* s390 */
	DEF_PAT_CPU( "cpu MHz static",	PAT_MHZ_STATIC,	static_mhz),	/* s390 */
	DEF_PAT_CPU( "cpu MHz",		PAT_MHZ,	mhz),
	DEF_PAT_CPU( "cpu number",	PAT_PROCESSOR,  logical_id),	/* s390 */
        DEF_PAT_CPU( "processor",	PAT_PROCESSOR,	logical_id),
};

#define CPUTYPE_PATTERN_BUFSZ	32

static int cmp_pattern(const void *a0, const void *b0)
{
	const struct cpuinfo_pattern
		*a = (const struct cpuinfo_pattern *) a0,
		*b = (const struct cpuinfo_pattern *) b0;
	return strcmp(a->pattern, b->pattern);
}

static int cpuinfo_parse_line(	struct lscpu_cputype **ct,
				struct lscpu_cpu **cpu,
				const char *str)
{
	struct cpuinfo_pattern key, *pat;
	const char *p, *v;
	char buf[CPUTYPE_PATTERN_BUFSZ] = { 0 }, **data;
	void *stru = NULL;

	DBG(GATHER, ul_debug("parse \"%s\"", str));

	if (!str || !*str)
		return -EINVAL;
	p = skip_blank(str);
	if (!p || !*p)
		return -EINVAL;

	v = strchr(p, ':');
	if (!v || !*v)
		return -EINVAL;

	/* prepare name of the field */
	xstrncpy(buf, p, sizeof(buf));
	buf[v - p] = '\0';
	v++;

	rtrim_whitespace((unsigned char *)buf);

	/* search in cpu-types patterns */
	key.pattern = buf;
	pat = bsearch(&key, type_patterns,
			ARRAY_SIZE(type_patterns),
			sizeof(struct cpuinfo_pattern),
			cmp_pattern);
	if (pat) {
		/* CPU type */
		if (!*ct)
			*ct = lscpu_new_cputype();
		stru = *ct;
	} else {
		/* search in cpu patterns */
		pat = bsearch(&key, cpu_patterns,
			ARRAY_SIZE(cpu_patterns),
			sizeof(struct cpuinfo_pattern),
			cmp_pattern);
		if (pat) {
			if (!*cpu)
				*cpu = lscpu_new_cpu();
			stru = *cpu;
		}
	}

	if (!stru) {
		DBG(GATHER, ul_debug("'%s' not found", buf));
		return 1;
	}

	/* prepare value */
	v = skip_space(v);
	if (!v || !*v)
		return -EINVAL;

	/* copy value to struct */
	switch (pat->id) {
	case PAT_PROCESSOR:
		(*cpu)->logical_id = atoi(v);
		break;
	default:
		/* set value as a string and cleanup */
		strdup_to_offset(stru, pat->offset, v);
		data = (char **) ((char *) stru + pat->offset);
		rtrim_whitespace((unsigned char *) *data);
		break;
	}

	return 0;
}

int lscpu_read_cpuinfo(struct lscpu_cxt *cxt)
{
	struct lscpu_cputype *type = NULL;
	struct lscpu_cpu *cpu = NULL;
	FILE *fp;
	char buf[BUFSIZ];

	DBG(GATHER, ul_debugobj(cxt, "reading cpuinfo"));

	fp = ul_path_fopen(cxt->procfs, "r", "cpuinfo");
	if (!fp)
		err(EXIT_FAILURE, _("cannot open %s"), "/proc/cpuinfo");

	do {
		const char *p = NULL;

		if (fgets(buf, sizeof(buf), fp) != NULL)
			p = skip_space(buf);

		if (p == NULL || (*buf && !*p)) {
			if (cpu)
				lscpu_add_cpu(cxt, cpu, type);
			else if (type) {
				/* Generic non-cpu data. For some architectures
				 * cpuinfo contains description block (at the
				 * beginning of the file (IBM s390) or at the
				 * end of the file (IBM POWER). The block is
				 * global for all CPUs.
				 */
				if (cxt->ncputypes == 1) {
					/* The type already exist, merge it. For example on POWER
					 * CPU list contains "cpu:" line with architecture and
					 * global information at the end of the file */
					struct lscpu_cputype *dflt = lscpu_cputype_get_default(cxt);
					if (dflt)
						lscpu_merge_cputype(dflt, type);
				} else
					lscpu_add_cputype(cxt, type);
			}

			lscpu_unref_cpu(cpu);
			lscpu_unref_cputype(type);
			cpu = NULL, type = NULL;

			if (p == NULL)
				break;	/* fgets() returns nothing; EOF */
		} else {
			rtrim_whitespace((unsigned char *) buf);
			cpuinfo_parse_line(&type, &cpu, p);
		}
	} while (1);

	lscpu_unref_cpu(cpu);
	lscpu_unref_cputype(type);
	fclose(fp);
	return 0;
}

#ifdef TEST_PROGRAM_CPUTYPE
/* TODO: move to lscpu.c */
struct lscpu_cxt *lscpu_new_context(void)
{
	return xcalloc(1, sizeof(struct lscpu_cxt));
}

void lscpu_free_context(struct lscpu_cxt *cxt)
{
	size_t i;

	if (!cxt)
		return;

	DBG(MISC, ul_debugobj(cxt, "freeing context"));

	DBG(MISC, ul_debugobj(cxt, " de-initialize paths"));
	ul_unref_path(cxt->syscpu);
	ul_unref_path(cxt->procfs);

	DBG(MISC, ul_debugobj(cxt, " freeing cpus"));
	for (i = 0; i < cxt->ncpus; i++)
		lscpu_unref_cpu(cxt->cpus[i]);

	DBG(MISC, ul_debugobj(cxt, " freeing types"));
	for (i = 0; i < cxt->ncputypes; i++)
		lscpu_unref_cputype(cxt->cputypes[i]);

	free(cxt->cputypes);
	free(cxt->cpus);
	free(cxt);
}

int main(int argc, char **argv)
{
	struct lscpu_cxt *cxt;

	cxt = lscpu_new_context();

	if (argc == 3 && strcmp(argv[1], "--prefix") == 0)
		cxt->prefix = argv[2];

	lscpu_init_debug();
	context_init_paths(cxt);

	lscpu_read_cpuinfo(cxt);

	lscpu_free_context(cxt);
	return EXIT_SUCCESS;
}
#endif /* TEST_PROGRAM_CPUTYPES */
