#include <sys/modctl.h>
#include <sys/cmn_err.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/errno.h>
#include <sys/open.h>
#include <sys/cred.h>
#include <sys/uio.h>
#include <sys/devops.h>
#include <sys/conf.h>
static int dummy_attach(dev_info_t *dip, ddi_attach_cmd_t cmd);
static int dummy_detach(dev_info_t *dip, ddi_detach_cmd_t cmd);
static int dummy_getinfo(dev_info_t *dip, ddi_info_cmd_t cmd, void *arg,
    void **resultp);
/* static int dummy_prop_op(dev_t dev, dev_info_t *dip, ddi_prop_op_t prop_op,
    int flags, char *name, caddr_t valuep, int *lengthp);
*/
static int dummy_open(dev_t *devp, int flag, int otyp, cred_t *cred);
dev_info_t *dummy_dip;

