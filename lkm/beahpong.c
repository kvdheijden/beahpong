#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/fb.h>
#include <linux/gpio/consumer.h>
#include <linux/delay.h>

#define DRIVER_NAME "beahpong"

#define T0H	350	// ns
#define T1H	900	// ns
#define T0L	900	// ns
#define T1L	350	// ns
#define	RES	50	// us

#define BPP 	24

#define DEFAULT_WIDTH 30
static unsigned int width = DEFAULT_WIDTH;
module_param(width, uint, 0);
MODULE_PARM_DESC(width, "width in pixels of the connected LED grid");

#define DEFAULT_HEIGHT 18
static unsigned int height = DEFAULT_HEIGHT;
module_param(height, uint, 0);
MODULE_PARM_DESC(height, "height in pixels of the connected LED grid");

#define DEFAULT_REFRESH_RATE 30
static unsigned int refresh_rate = DEFAULT_REFRESH_RATE;
module_param(refresh_rate, uint, 0);
MODULE_PARM_DESC(refresh_rate, "rate at which the LED grid is refreshed");

struct beahpong_par {
	struct gpio_desc *dout_fb;
	struct gpio_desc *dout_p1;
	struct gpio_desc *dout_p2;

	u32 pseudo_palette[16];
};

static void beahpong_reset(struct beahpong_par *par)
{
	gpiod_set_value(par->dout_fb, 0);
	gpiod_set_value(par->dout_p1, 0);
	gpiod_set_value(par->dout_p2, 0);
	usleep_range(RES, RES + 1);
}

static void beahpong_set(struct beahpong_par *par, bool hi)
{
	gpiod_set_value(par->dout_fb, 1);
	ndelay(hi ? T1H : T0H);
	gpiod_set_value(par->dout_fb, 0);
	ndelay(hi ? T1L : T0L);
}

static void beahpong_deferred_io(struct fb_info *info, struct list_head *pagelist)
{
	int i, j;
	struct beahpong_par *par = info->par;
	u8 *vmem = info->screen_base;

	beahpong_reset(par);
	for (i = 0; i < info->fix.smem_len; i++) {
		for (j = 0; j < 8; j++) {
			beahpong_set(par, (vmem[i] >> j) & 0x01);
		}
	}
}

static unsigned int chan_to_field(unsigned chan, struct fb_bitfield *bf)
{
	chan &= 0xFFFF;
	chan >>= 16 - bf->length;
	return chan << bf->offset;
}

static ssize_t beahpong_read(struct fb_info *info, char __user *buf, size_t count, loff_t *ppos)
{
#if defined(CONFIG_FB_SYS_FOPS) || defined(CONFIG_FB_SYS_FOPS_MODULE)
	return fb_sys_read(info, buf, count, ppos);
#else
#error "Missing implementation for fb_sys_read(info, buf, count, ppos);"
#endif
}

static ssize_t beahpong_write(struct fb_info *info, const char __user *buf, size_t count, loff_t *ppos)
{
#if defined(CONFIG_FB_SYS_FOPS) || defined(CONFIG_FB_SYS_FOPS_MODULE)
	return fb_sys_write(info, buf, count, ppos);
#else
#error "Missing implementation for fb_sys_write(info, buf, count, ppos);"
#endif
}

static int beahpong_setcolreg(unsigned regno, unsigned red, unsigned green, unsigned blue, unsigned transp, struct fb_info *info)
{
	if (info->fix.visual == FB_VISUAL_TRUECOLOR && regno < 16) {
		u32 *palette = info->pseudo_palette;

		palette[regno]  = chan_to_field(red,   &info->var.red);
		palette[regno] |= chan_to_field(green, &info->var.green);
		palette[regno] |= chan_to_field(blue,  &info->var.blue);

		return 0;
	}

	return -EINVAL;
}

static int beahpong_blank(int blank, struct fb_info *info)
{
	return -EINVAL;
}

static void beahpong_fillrect(struct fb_info *info, const struct fb_fillrect *rect)
{
#if defined(CONFIG_FB_SYS_FILLRECT) || defined(CONFIG_FB_SYS_FILLRECT_MODULE)
	sys_fillrect(info, rect);
#elif defined(CONFIG_FB_CFB_FILLRECT) || defined(CONFIG_FB_CFB_FILLRECT_MODULE)
	cfb_fillrect(info, rect);
#else
#error "Missing implementation for fillrect(info, rect);"
#endif
}

static void beahpong_copyarea(struct fb_info *info, const struct fb_copyarea *region)
{
#if defined(CONFIG_FB_SYS_COPYAREA) || defined(CONFIG_FB_SYS_COPYAREA_MODULE)
	sys_copyarea(info, region);
#elif defined(CONFIG_FB_CFB_COPYAREA) || defined(CONFIG_FB_CFB_COPYAREA_MODULE) 
	cfb_copyarea(info, region);
#else
#error "Missing implementation for copyarea(info, region);"
#endif
}

static void beahpong_imageblit(struct fb_info *info, const struct fb_image *image)
{
#if defined(CONFIG_FB_SYS_IMAGEBLIT) || defined(CONFIG_FB_SYS_IMAGEBLIT_MODULE)
	sys_imageblit(info, image);
#elif defined(CONFIG_FB_CFB_IMAGEBLIT) || defined(CONFIG_FB_CFB_IMAGEBLIT_MODULE)
	cfb_imageblit(info, image);
#else
#error "Missing implementation for imageblit(info, image);"
#endif
}

static struct fb_info *beahpong_framebuffer_alloc(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct gpio_desc *dout_fb = NULL, *dout_p1 = NULL, *dout_p2 = NULL;
	struct fb_info *info = NULL;
	struct beahpong_par *par = NULL;
	struct fb_ops *fbops = NULL;
	struct fb_deferred_io *fbdefio = NULL;
	u8 *vmem = NULL;
	u32 vmem_size = 0;

	// Get device parameters
	dout_fb = devm_gpiod_get(dev, "dout_fb", GPIOD_OUT_HIGH);
	if (IS_ERR(dout_fb)) {
		dev_err(dev, "Failed to get GPIO [dout_fb]: %ld\n", PTR_ERR(dout_fb));
		goto vmem_alloc_failed;
	}
	dout_p1 = devm_gpiod_get(dev, "dout_p1", GPIOD_OUT_HIGH);
	if (IS_ERR(dout_p1)) {
		dev_err(dev, "Failed to get GPIO [dout_p1]: %ld\n", PTR_ERR(dout_p1));
		goto vmem_alloc_failed;
	}
	dout_p2 = devm_gpiod_get(dev, "dout_p2", GPIOD_OUT_HIGH);
	if (IS_ERR(dout_p2)) {
		dev_err(dev, "Failed to get GPIO [dout_p2]: %ld\n", PTR_ERR(dout_p2));
		goto vmem_alloc_failed;
	}

	vmem_size = width * height * BPP / 8;
	if (!vmem_size) {
		dev_err(dev, "Trying to create screen with 0 pixels.\n");
		goto vmem_alloc_failed;
	}

	vmem = vzalloc(vmem_size);
	if (!vmem) {
		dev_err(dev, "Failed to allocate vmem[%d]\n", vmem_size);
		goto vmem_alloc_failed;
	}
	
	fbops = devm_kzalloc(dev, sizeof(struct fb_ops), GFP_KERNEL);
	if (!fbops) {
		dev_err(dev, "Failed to allocate fbops\n");
		goto alloc_failed;
	}

	fbdefio = devm_kzalloc(dev, sizeof(struct fb_deferred_io), GFP_KERNEL);
	if (!fbdefio) {
		dev_err(dev, "Failed to allocate fbdefio\n");
		goto alloc_failed;
	}

	info = framebuffer_alloc(sizeof(struct beahpong_par), dev);
	if (!info) {
		dev_err(dev, "Failed to allocate framebuffer\n");
		goto alloc_failed;
	}

	info->screen_base = (u8 __force __iomem *) vmem;
	info->fbops = fbops;
	info->fbdefio = fbdefio;

	fbops->owner        = dev->driver->owner;
	fbops->fb_read      = beahpong_read;
	fbops->fb_write     = beahpong_write;
	fbops->fb_setcolreg = beahpong_setcolreg;
	fbops->fb_blank     = beahpong_blank;
	fbops->fb_fillrect  = beahpong_fillrect;
	fbops->fb_copyarea  = beahpong_copyarea;
	fbops->fb_imageblit = beahpong_imageblit;

	fbdefio->delay       = (refresh_rate > 0) ? (HZ/refresh_rate) : (0);
	fbdefio->deferred_io = beahpong_deferred_io;
	fb_deferred_io_init(info);

	strncpy(info->fix.id, dev->driver->name, 16);
	info->fix.type        = FB_TYPE_PACKED_PIXELS;
	info->fix.visual      = FB_VISUAL_TRUECOLOR;
	info->fix.xpanstep    = 0;
	info->fix.ypanstep    = 0;
	info->fix.ywrapstep   = 0;
	info->fix.line_length = width * BPP / 8;
	info->fix.accel       = FB_ACCEL_NONE;
	info->fix.smem_len    = vmem_size;

	info->var.rotate         = 0;
	info->var.xres_virtual   = info->var.xres = width;
	info->var.yres_virtual   = info->var.yres = height;
	info->var.bits_per_pixel = BPP;
	info->var.nonstd         = 1;

	// GRB888, high bit first
	info->var.red.offset     = 8;
	info->var.red.length     = 8;
	info->var.green.offset   = 0;
	info->var.green.length   = 8;
	info->var.blue.offset    = 16;
	info->var.blue.length    = 8;
	info->var.transp.offset  = 0;
	info->var.transp.length  = 0;

	info->flags              = FBINFO_FLAG_DEFAULT | FBINFO_VIRTFB;

	par = info->par;
	par->dout_fb = dout_fb;
	par->dout_p1 = dout_p1;
	par->dout_p2 = dout_p2;

	info->pseudo_palette     = par->pseudo_palette;

	return info;

alloc_failed:
	vfree(vmem);
vmem_alloc_failed:
	return NULL;
}

static void beahpong_framebuffer_release(struct fb_info *info)
{
	fb_deferred_io_cleanup(info);
	vfree(info->screen_base);
	framebuffer_release(info);
}

static int beahpong_register_framebuffer(struct platform_device *pdev, struct fb_info *info)
{
	struct device *dev = &pdev->dev;
	int status = 0;

	status = register_framebuffer(info);
	if (status < 0) {
		dev_err(dev, "Failed to register framebuffer: %d\n", status);
		return status;
	}

	platform_set_drvdata(pdev, info);

	return 0;
}

static int beahpong_unregister_framebuffer(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct fb_info *info = NULL;

	info = platform_get_drvdata(pdev);
	if (!info) {
		dev_err(dev, "Failed to retrieve framebuffer info\n");
		return -EINVAL;
	}

	unregister_framebuffer(info);
	platform_set_drvdata(pdev, NULL);

	// Return succesfully
	return 0;
}

static int beahpong_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct fb_info *info = NULL;
	int status = 0;

	info = beahpong_framebuffer_alloc(pdev);
	if (!info) {
		dev_err(dev, "Failed to allocate framebuffer\n");
		status = -ENOMEM;
		goto allocate_failed;
	}

	status = beahpong_register_framebuffer(pdev, info);
	if (status) {
		dev_err(dev, "Failed to register framebuffer: %d\n", status);
		goto register_failed;
	}

	dev_info(dev, "Device probed successfully\n");

	// Return succesfully
	return 0;

	// Error handling
register_failed:
	beahpong_framebuffer_release(info);
allocate_failed:
	return status;
}

static int beahpong_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct fb_info *info = NULL;
	int status = 0;

	info = platform_get_drvdata(pdev);
	if (!info) {
		dev_err(dev, "Failed to retrieve framebuffer info\n");
		return -EINVAL;
	}

	status = beahpong_unregister_framebuffer(pdev);
	if (status < 0) {
		dev_err(dev, "Failed to unregister framebuffer: %d\n", status);
		return status;
	}

	beahpong_framebuffer_release(info);

	dev_info(dev, "Device removed succesfully\n");

	// Return succesfully
	return 0;
}

#if defined(CONFIG_OF)
static const struct of_device_id beahpong_dt_ids[] = {
	{ .compatible = "beahpong" },
	{}
};
MODULE_DEVICE_TABLE(of, beahpong_dt_ids);
#endif

static struct platform_driver beahpong_driver = {
	.probe = beahpong_probe,
	.remove = beahpong_remove,
	.driver = {
		.name = DRIVER_NAME,
		.of_match_table = of_match_ptr(beahpong_dt_ids)
	}
};

module_platform_driver(beahpong_driver);

MODULE_AUTHOR("Koen van der Heijden <koen@kvdheijden.com>");
MODULE_DESCRIPTION("Driver for the Beahpong table");
MODULE_LICENSE("GPL");
