#include "./dummy.h"

/* cb_ops structure */
static struct cb_ops dummy_cb_ops = {
    dummy_open,
    nodev,
    nodev,              /* no strategy - nodev returns ENXIO */
    nodev,              /* no print */
    nodev,              /* no dump */
    nodev,
    nodev,
    nodev,              /* no ioctl */
    nodev,              /* no devmap */
    nodev,              /* no mmap */
    nodev,              /* no segmap */
    nochpoll,           /* returns ENXIO for non-pollable devices */
    nodev,
    NULL,               /* streamtab struct; if not NULL, all above */
                        /* fields are ignored */
    D_NEW | D_MP,       /* compatibility flags: see conf.h */
    CB_REV,             /* cb_ops revision number */
    nodev,              /* no aread */
    nodev               /* no awrite */
};

/* dev_ops structure */
static struct dev_ops dummy_dev_ops = {
    DEVO_REV,
    0,                         /* reference count */
    dummy_getinfo,             /* no getinfo(9E) */
    nulldev,                   /* no identify(9E) - nulldev returns 0 */
    nulldev,                   /* no probe(9E) */
    dummy_attach,
    dummy_detach,
    nodev,                     /* no reset - nodev returns ENXIO */
    &dummy_cb_ops,
    (struct bus_ops *)NULL,
    nodev,                     /* no power(9E) */
    ddi_quiesce_not_needed,    /* no quiesce(9E) */
};

/* modldrv structure */
static struct modldrv md = {
    &mod_driverops,     /* Type of module. This is a driver. */
    "dummy driver",     /* Name of the module. */
    &dummy_dev_ops
};

/* modlinkage structure */
static struct modlinkage ml = {
    MODREV_1,
    &md,
    NULL
};



static int
dummy_attach(dev_info_t *dip, ddi_attach_cmd_t cmd)
{
	cmn_err(CE_NOTE, "Inside dummy_attach");
	switch(cmd) {
	case DDI_ATTACH:
		dummy_dip = dip;
		if (ddi_create_minor_node(dip, "0", S_IFCHR,
		    ddi_get_instance(dip), DDI_PSEUDO, 0) != DDI_SUCCESS) {
			cmn_err(CE_NOTE, "dummy_attach");
			return DDI_FAILURE;
		} else
			return DDI_SUCCESS;
	default:
			return DDI_FAILURE;
			
	}
}
	
static int
dummy_detach(dev_info_t *dip, ddi_detach_cmd_t cmd)
{
	cmn_err(CE_NOTE, "Inside dummy_detach");
	switch(cmd) {
		case DDI_DETACH:
			dummy_dip = 0;
			ddi_remove_minor_node(dip, NULL);
			return DDI_SUCCESS;
		default:
			return DDI_FAILURE;
	}
}
	
static int
dummy_getinfo(dev_info_t *dip, ddi_info_cmd_t cmd, void *arg, void **resultp)
{
	cmn_err(CE_NOTE, "inside dummy_getinfo");
	switch(cmd) {
		case DDI_INFO_DEVT2DEVINFO:
			*resultp = dummy_dip;
			return DDI_SUCCESS;
		case DDI_INFO_DEVT2INSTANCE:
			*resultp = 0;
			return DDI_SUCCESS;
		default:
			return DDI_FAILURE;
	}
}

static int
dummy_open(dev_t *devp, int flag, int otyp, cred_t *cred)
{
	cmn_err(CE_NOTE, "Inside dummy_open");
	return DDI_SUCCESS;
}

int
_init()
{
	cmn_err(CE_NOTE, "Inside _init");
	return(mod_install(&ml));
}

int
_info(struct modinfo *modinfop)
{
	cmn_err(CE_NOTE, "Inside _info");
	return(mod_info(&ml, modinfop));	
}

int
_fini()
{
	cmn_err(CE_NOTE, "Inside _fini");
	return(mod_remove(&ml));
}

