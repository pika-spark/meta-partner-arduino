/**
 * TCPM: USB Type-C Port Controller Manager
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/usb/pd.h>
#include <linux/usb/tcpm.h>
#include <linux/usb/typec.h>

#include "anx7625.h"


#define OCM_DEBUG_REG_8             0x88
#define STOP_MAIN_OCM           6


#define DEBUG 1

#ifdef DEBUG
#define DBG_PRINT(fmt, ...) 	printk(KERN_ERR "TCPM:%s:"fmt, __func__, ##__VA_ARGS__)
#endif

extern struct anx7625_data *anx7625_ctx;


int anx7625_reg_read(struct anx7625_data *ctx,
			    struct i2c_client *client, u8 reg_addr);
int anx7625_reg_block_read(struct anx7625_data *ctx,
				  struct i2c_client *client,
				  u8 reg_addr, u8 len, u8 *buf);
int anx7625_reg_write(struct anx7625_data *ctx,
			     struct i2c_client *client, u8 reg_addr, u8 reg_val);
int anx7625_write_or(struct anx7625_data *ctx, struct i2c_client *client,
			    u8 offset, u8 mask);
int anx7625_write_and(struct anx7625_data *ctx,
			     struct i2c_client *client, u8 offset, u8 mask);
int anx7625_write_and_or(struct anx7625_data *ctx,
				struct i2c_client *client, u8 offset,
				u8 and_mask, u8 or_mask);




struct anx7625_tcpm {
	struct device       *dev;
	struct tcpm_port    *port;
	bool                 controls_vbus;
	struct tcpc_dev      tcpc;
	int                  vbus;
	enum typec_cc_status cc1;
	enum typec_cc_status cc2;
};

struct anx7625_tcpm *anx7625_tcpm;


static int anx7625_tcpm_init(struct tcpc_dev *tcpc);
static int anx7625_tcpm_get_vbus(struct tcpc_dev *tcpc);
static int anx7625_tcpm_get_cc(struct tcpc_dev *tcpc,
			enum typec_cc_status *cc1, enum typec_cc_status *cc2);
static int anx7625_tcpm_set_cc(struct tcpc_dev *tcpc, enum typec_cc_status cc);
static int anx7625_tcpm_set_vbus(struct tcpc_dev *tcpc, bool source, bool sink);
static int anx7625_tcpm_set_polarity(struct tcpc_dev *tcpc,
		enum typec_cc_polarity polarity);
static int anx7625_tcpm_set_vconn(struct tcpc_dev *tcpc, bool enable);
static int anx7625_tcpm_set_pd_rx(struct tcpc_dev *tcpc, bool enable);
static int anx7625_tcpm_set_roles(struct tcpc_dev *tcpc, bool attached,
			   enum typec_role role, enum typec_data_role data);
static int anx7625_tcpm_pd_transmit(struct tcpc_dev *tcpc,
			     enum tcpm_transmit_type type,
			     const struct pd_message *msg);

/**
 */
int anx7625_tcpm_probe(void)
{
	struct device *dev = &anx7625_ctx->client->dev;
DBG_PRINT("start\n");
	anx7625_tcpm = devm_kzalloc(dev, sizeof(*anx7625_tcpm), GFP_KERNEL);
	if (!anx7625_tcpm) {
		return -ENOMEM;
	}

	anx7625_tcpm->dev = dev;
	//anx7625_tcpm->data = data;
	//anx7625_tcpm->regmap = data->regmap;
/* TODO

	anx7625_tcpm->tcpc.start_toggling = anx7625_tcpm_start_toggling;

	
	*/

/* TODO vanno definite

	    !tcpc->get_vbus ||
	    !tcpc->set_cc ||
	    !tcpc->get_cc ||
	    !tcpc->set_polarity || 
	    !tcpc->set_vconn ||
	    !tcpc->set_vbus ||
	    !tcpc->set_pd_rx ||
	    !tcpc->set_roles ||
	    !tcpc->pd_transmit)
	    
	    */

	anx7625_tcpm->tcpc.init         = anx7625_tcpm_init;
	anx7625_tcpm->tcpc.get_vbus     = anx7625_tcpm_get_vbus;
	anx7625_tcpm->tcpc.set_vbus     = anx7625_tcpm_set_vbus;
	anx7625_tcpm->tcpc.set_cc       = anx7625_tcpm_set_cc;
	anx7625_tcpm->tcpc.get_cc       = anx7625_tcpm_get_cc;
	anx7625_tcpm->tcpc.set_polarity = anx7625_tcpm_set_polarity;
	anx7625_tcpm->tcpc.set_vconn    = anx7625_tcpm_set_vconn;
	anx7625_tcpm->tcpc.set_pd_rx    = anx7625_tcpm_set_pd_rx;
	anx7625_tcpm->tcpc.set_roles    = anx7625_tcpm_set_roles;
	anx7625_tcpm->tcpc.pd_transmit  = anx7625_tcpm_pd_transmit;
	
	anx7625_tcpm->controls_vbus = true; /* XXX */

	anx7625_tcpm->tcpc.fwnode = device_get_named_child_node(dev, "connector");
	if (!anx7625_tcpm->tcpc.fwnode) {
		DBG_PRINT("ERROR Can't find connector node\n");
		dev_err(dev, "Can't find connector node.\n");
		return -EINVAL;
	}
	DBG_PRINT("Connector node found\n");

	anx7625_tcpm->port = tcpm_register_port(dev, &anx7625_tcpm->tcpc);
	if (IS_ERR(anx7625_tcpm->port)) {
		DBG_PRINT("ERROR on tcpm_register_port\n");

		return -1;// ERR_CAST(anx7625_tcpm->port);
	}

	DBG_PRINT("DONE\n");
	return 0;
}

/**
 */
int anx7625_tcpm_release(void)
{
DBG_PRINT("\n");
	tcpm_unregister_port(anx7625_tcpm->port);
	return 0;
}

/**
 */
int anx7625_tcpm_change(int sys_status, int ivector, int cc_status)
{
	switch (cc_status & 0x0F) {
	case  0: anx7625_tcpm->cc1 = TYPEC_CC_OPEN  ; break;
	case  1: anx7625_tcpm->cc1 = TYPEC_CC_RD    ; break;
	case  2: anx7625_tcpm->cc1 = TYPEC_CC_RA    ; break;
	case  4: anx7625_tcpm->cc1 = TYPEC_CC_RP_DEF; break;
	case  8: anx7625_tcpm->cc1 = TYPEC_CC_RP_1_5; break;
	case 12: anx7625_tcpm->cc1 = TYPEC_CC_RP_3_0; break;
	default:
		printk("anx: CC1: Reserved\n");
	}

	switch (cc_status & 0xF0) {
	case  0: anx7625_tcpm->cc2 = TYPEC_CC_OPEN  ; break;
	case  1: anx7625_tcpm->cc2 = TYPEC_CC_RD    ; break;
	case  2: anx7625_tcpm->cc2 = TYPEC_CC_RA    ; break;
	case  4: anx7625_tcpm->cc2 = TYPEC_CC_RP_DEF; break;
	case  8: anx7625_tcpm->cc2 = TYPEC_CC_RP_1_5; break;
	case 12: anx7625_tcpm->cc2 = TYPEC_CC_RP_3_0; break;
	default:
		printk("anx: CC2: Reserved\n");
	}

	anx7625_tcpm->vbus = (sys_status & BIT(3))? 1: 0;  // TODO to verify

	if (ivector & BIT(4)) {
		//CC status change
		tcpm_cc_change(anx7625_tcpm->port);
	}
	
	if (ivector & BIT(3)) {
		//VBUS change
		tcpm_vbus_change(anx7625_tcpm->port);
		/*
		// TODO to verify
		if (anx7625_tcpm->vbus) {
			tcpm_vbus_change(anx7625_tcpm->port);
		} else {
			tcpm_tcpc_reset(anx7625_tcpm->port);
		}*/
	}
	return 0;
}

/**
 */
static int anx7625_tcpm_init(struct tcpc_dev *tcpc)
{
	DBG_PRINT("\n");
	return 0;
}

/**
 */
static int anx7625_tcpm_get_vbus(struct tcpc_dev *tcpc)
{
	/* TODO
	ret = regmap_read(tcpci->regmap, TCPC_POWER_STATUS, &reg);
	if (ret < 0)
		return ret;

	return !!(reg & TCPC_POWER_STATUS_VBUS_PRES);
	*/
	DBG_PRINT("vbus %d\n", anx7625_tcpm->vbus);
	return anx7625_tcpm->vbus;
}

/**
 */
static int anx7625_tcpm_set_cc(struct tcpc_dev *tcpc, enum typec_cc_status cc)
{
	/*
enum typec_cc_status {
	TYPEC_CC_OPEN,
	TYPEC_CC_RA,
	TYPEC_CC_RD,
	TYPEC_CC_RP_DEF,
	TYPEC_CC_RP_1_5,
	TYPEC_CC_RP_3_0,
};

5:4 RP_VALUE 00: Rp default, 01: Rp 1.5A, 10: Rp 3.0A, 11:reserved
3:2 CC2_CONTROL 00: Ra, 01: Rp, 10: Rd, 11: Open.
1:0 CC1_CONTROL 00: Ra, 01: Rp, 10: Rd, 11: Open.
*/
	u8 reg;
	reg = anx7625_reg_read(anx7625_ctx, anx7625_ctx->i2c.tcpc_client,
											 TCPC_ROLE_CONTROL);
	reg &= 0xC0;
	switch (cc) {
	case TYPEC_CC_OPEN  : reg |= 0x0F; break;
	case TYPEC_CC_RA    : reg |= 0x00; break;
	case TYPEC_CC_RD    : reg |= 0x0A; break;
	case TYPEC_CC_RP_DEF: reg |= 0x05; break;
	case TYPEC_CC_RP_1_5: reg |= 0x15; break;
	case TYPEC_CC_RP_3_0: reg |= 0x25; break;
	}
//	DBG_PRINT("cc %d\n", cc);
	return 0;
}

/**
 */
static int anx7625_tcpm_get_cc(struct tcpc_dev *tcpc,
			enum typec_cc_status *cc1, enum typec_cc_status *cc2)
{
	*cc1 = anx7625_tcpm->cc1;
	*cc2 = anx7625_tcpm->cc2;
	DBG_PRINT("cc1 %d, cc2 %d\n", anx7625_tcpm->cc1, anx7625_tcpm->cc2);
	return 0;
}

/**
 */
static int anx7625_tcpm_set_vbus(struct tcpc_dev *tcpc, bool source, bool sink)
{
	DBG_PRINT("source %d, sink %d\n", source, sink);
	return 0;
}

/**
 */
static int anx7625_tcpm_set_polarity(struct tcpc_dev *tcpc,
		enum typec_cc_polarity polarity)
{
	/*
enum typec_cc_polarity {
	TYPEC_POLARITY_CC1,
	TYPEC_POLARITY_CC2,
};

	 */
	DBG_PRINT("polarity %d\n", polarity);
	return 0;
}

/**
 */
static int anx7625_tcpm_set_vconn(struct tcpc_dev *tcpc, bool enable)
{
	DBG_PRINT("enable %d\n", enable);
	return 0;
}

/**
 */
static int anx7625_tcpm_set_pd_rx(struct tcpc_dev *tcpc, bool enable)
{
	DBG_PRINT("enable %d\n", enable);
	return 0;
}

static int anx7625_tcpm_set_roles(struct tcpc_dev *tcpc, bool attached,
			   enum typec_role role, enum typec_data_role data)
{
	/*
	 
enum typec_data_role {
	TYPEC_DEVICE,
	TYPEC_HOST,
};

enum typec_role {
	TYPEC_SINK,
	TYPEC_SOURCE,
};
	 
	 */
	DBG_PRINT("attached %d, role %d, data %d\n", attached, role, data);
	return 0;
}

/**
 */
static int anx7625_tcpm_pd_transmit(struct tcpc_dev *tcpc,
			     enum tcpm_transmit_type type,
			     const struct pd_message *msg)
{
	DBG_PRINT("type %d\n", type);
	return 0;
}


/**
 *configure DPR toggle
 */
void anx7625_DRP_Enable(void)
{
	int ret;
	/*reset main OCM*/
	//WriteReg(RX_P0, OCM_DEBUG_REG_8, 1<<STOP_MAIN_OCM);
	ret = anx7625_reg_write(anx7625_ctx, anx7625_ctx->i2c.rx_p0_client,
					OCM_DEBUG_REG_8, 1<<STOP_MAIN_OCM);

	/*config toggle.*/
	//WriteReg(TCPC_INTERFACE, TCPC_ROLE_CONTROL, 0x45);
	//WriteReg(TCPC_INTERFACE, TCPC_COMMAND, 0x99);
	//WriteReg(TCPC_INTERFACE, TCPC_ANALOG_CTRL_1, 0xA0);
	//WriteReg(TCPC_INTERFACE, TCPC_ANALOG_CTRL_1, 0xE0);
	ret = anx7625_reg_write(anx7625_ctx, anx7625_ctx->i2c.tcpc_client,
					TCPC_ROLE_CONTROL, 0x45);
	ret = anx7625_reg_write(anx7625_ctx, anx7625_ctx->i2c.tcpc_client,
					TCPC_COMMAND, 0x99);
	ret = anx7625_reg_write(anx7625_ctx, anx7625_ctx->i2c.tcpc_client,
					TCPC_ANALOG_CTRL_1, 0xA0);
	ret = anx7625_reg_write(anx7625_ctx, anx7625_ctx->i2c.tcpc_client,
					TCPC_ANALOG_CTRL_1, 0xE0);

	
	DBG_PRINT("Enable DRP!");
}