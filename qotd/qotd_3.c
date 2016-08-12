#include <sys/types.h>
#include <sys/file.h>
#include <sys/errno.h>
#include <sys/open.h>
#include <sys/cred.h>
#include <sys/uio.h>
#include <sys/stat.h>
#include <sys/modctl.h>
#include <sys/conf.h>
#include <sys/devops.h>
#include <sys/debug.h>
#include <sys/cmn_err.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>

#define QOTD_NAME       "qotd"

static const char qotd[]
        = "You can't have everything. \
Where would you put it? - Steven Wright\n";
static const size_t init_qotd_len = 128;

#define QOTD_MAX_LEN	65535
#define QOTD_CHANGED	0x1
#define QOTD_DIDMINOR	0x2
#define QOTD_DIDALLOC	0x4
#define QOTD_DIDMUTEX	0x8
#define QOTD_DIDCV	0x10
#define QOTD_BUSY	0x20

static void *qotd_state_head;

typedef struct qotd_state {
	int		instance;
	dev_info_t	*devi;
	kmutex_t	lock;
	kcondvar_t	cv;
	char		*qotd;
	size_t		qotd_len;
	ddi_umem_cookie_t qotd_cookie;
	int		flags;
}qotd_state_t;

static int qotd_getinfo(dev_info_t *, ddi_info_cmd_t, void *, void **);
static int qotd_attach(dev_info_t *, ddi_attach_cmd_t);
static int qotd_detach(dev_info_t *, ddi_detach_cmd_t);
static int qotd_open(dev_t *, int, int, cred_t *);
static int qotd_close(dev_t, int, int, cred_t *);
static int qotd_read(dev_t, struct uio *, cred_t *);
static int qotd_write(dev_t, struct uio *, cred_t *);
static int qotd_rw(dev_t, struct uio *, enum uio_rw);
static int qotd_ioctl(dev_t, int, intptr_t, int, cred_t *, int *);

static struct cb_ops qotd_cb_ops = {
        qotd_open,              /* cb_open */
        qotd_close,             /* cb_close */
        nodev,                  /* cb_strategy */
        nodev,                  /* cb_print */
        nodev,                  /* cb_dump */
        qotd_read,              /* cb_read */
        qotd_write,                  /* cb_write */
        qotd_ioctl,                  /* cb_ioctl */
        nodev,                  /* cb_devmap */
        nodev,                  /* cb_mmap */
        nodev,                  /* cb_segmap */
        nochpoll,               /* cb_chpoll */
        ddi_prop_op,            /* cb_prop_op */
        (struct streamtab *)NULL,       /* cb_str */
        D_MP | D_64BIT,         /* cb_flag */
        CB_REV,                 /* cb_rev */
        nodev,                  /* cb_aread */
        nodev                   /* cb_awrite */
};

static struct dev_ops qotd_dev_ops = {
        DEVO_REV,               /* devo_rev */
        0,                      /* devo_refcnt */
        qotd_getinfo,           /* devo_getinfo */
        nulldev,                /* devo_identify */
        nulldev,                /* devo_probe */
        qotd_attach,            /* devo_attach */
        qotd_detach,            /* devo_detach */
        nodev,                  /* devo_reset */
        &qotd_cb_ops,           /* devo_cb_ops */
        (struct bus_ops *)NULL, /* devo_bus_ops */
        nulldev,                /* devo_power */
        ddi_quiesce_not_needed, /* devo_quiesce */
};

static struct modldrv modldrv = {
        &mod_driverops,
        "Quote of the Day 2.0",
        &qotd_dev_ops};

static struct modlinkage modlinkage = {
        MODREV_1,
        (void *)&modldrv,
        NULL
};

int
_init(void)
{
	int retval;
	
	if ((retval = ddi_soft_state_init(&qotd_state_head, 
	    sizeof (struct qotd_state), 1)) != 0) {
		return retval;
	}

	if((retval = mod_install(&modlinkage)) != 0) {
		ddi_soft_state_fini(&qotd_state_head);
		return retval;
	}
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}

int
_fini(void)
{
	int retval;
	
	if ((retval = mod_remove(&modlinkage) != 0))
		return retval;
	ddi_soft_state_fini(&qotd_state_head);
	
	return retval;
}

static int
qotd_attach(dev_info_t *dip, ddi_attach_cmd_t cmd)
{
	int instance = ddi_get_instance(dip);
	struct qotd_state *qsp;

	switch (cmd) {
	case DDI_ATTACH:
		ddi_soft_state_zalloc(qotd_state_head, instance);
		qsp = ddi_get_soft_state(qotd_state_head, instance);
		ddi_create_minor_node(dip, QOTD_NAME, S_IFCHR, instance,
		    DDI_PSEUDO, 0);
		qsp->instance = instance;
		qsp->devi = dip;
		qsp->flags |= QOTD_DIDMINOR;
		qsp->qotd = ddi_umem_alloc(init_qotd_len, DDI_UMEM_NOSLEEP,
		    &qsp->qotd_cookie);
		qsp->flags |= QOTD_DIDALLOC;
		mutex_init(&qsp->lock, NULL, MUTEX_DRIVER, NULL);
		qsp->flags |= QOTD_DIDMUTEX;
		cv_init(qsp->cv, NULL, CV_DRIVER, NULL);
		qsp->flags |= QOTD_DIDCV;
		strlcpy(qsp->qotd, init_qotd, init_qotd_len);
		qsp->qotd_len = init_qotd_len;
		ddi_report_dev(dip);
		return DDI_SUCCESS;
	case DDI_RESUME:
		return DDI_SUCCESS;
	default:
		return DDI_FAILURE;
	}
}

static int
qotd_detach(dev_info_t *dip, ddi_detach_cmd_t cmd)
{
	int instance = ddi_get_instance(dip);
	struct qotd_state qsp;
	switch (cmd) {
	case DDI_DETACH:
		qsp = ddi_get_soft_state(qotd_state_head, instance);
		ASSERT(!(qsp->flags & QOTD_BUSY));
		if (qsp->flags & QOTD_CHANGED)
			return EBUSY ;

		ddi_soft_state_free(qotd_state_head, instance);
		ddi_remove_minor_node(dip, NULL);
		return DDI_SUCCESS;
	case DDI_SUSPEND:
		return DDI_SUCCESS;
	default:
		return DDI_FAILURE;
	}
}


static int
qotd_getinfo(dev_info_t *dip ddi_info_cmd_t cmd, void *arg, void **resultp)
{
	struct qotd_state_t *qsp;
	int retval = DDI_FAILURE;

	ASSERT(resultp != NULL);

	switch (cmd) {
		case DDI_INFO_DEVT2DEVINFO:
			qsp = ddi_get_soft_state(qotd_state_head,
			    getminor((dev_t)arg));
			*resultp = qsp->devi;
			retval = DDI_SUCCESS;
			break;
		case DDI_INFO_DEVT2INSTANCE:
			*resultp = (void *)getminor((dev_t)arg);
			retval = DDI_SUCCESS;
			break;
	}
	return retval;
}

static int
qotd_open(dev_t *devp, int flag, int otyp, cred_t *credp)
{
	return 0;
}

static int
qotd_close(dev_t dev, int flag, int otyp, cred_t *credp)
{
	return 0;
}

static int
qotd_read(dev_t dev, struct uio *uiop, cred_t *credp)
{
	return 0;
}

static int
qotd_ioctl(dev_t dev, int cmd, intptr_t arg, int mode, cred_t *credp, int *rvalp)
{
	struct qotd_state *qsp;
	int instance = getminor(dev);
	qsp = ddi_get_soft_state(qotd_state_head, instance);

	switch (cmd) {
	case QOTDIOCGSZ: {
		size_t sz = qsp->qotd_len;
		if (!(mode & FREAD))
			return EACCES;
	#ifdef _MULTI_DATAMODEL
		return EFAULT;
	#else
		if (ddi_copyout(&sz, (void *)arg, sizeof (size_t), mode) != 0)
			return EFAULT;
		return 0;
	#endif
	case QOTDIOCSSZ:
		size_t new_len;
		char *new_qotd;
		ddi_umem_cookie_t new_cookie;
		uint_t model;
		if (ddi_copyin((void *)arg, &new_len, sizeof (size_t), 
		    mode) != 0)
			return EFAULT;
		if (new_len == 0 || new_len > QOTD_MAX_LEN)
			return EFAULT;
		new_qotd = ddi_umem_alloc(new_len, DDI_UMEM_SLEEP, *new_cookie);

		mutex_enter(&qsp->lock);
		
		while (qsp->flags & QOTD_BUSY) {
			if (cv_wait_sig(&qsp->cv, &qsp->lock) == 0) {
				mutex_exit(&qsp->lock);
			}

		}
	
			 
			 }
	
	
	}


}
