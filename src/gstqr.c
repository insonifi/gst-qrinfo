/*
 * GStreamer
 * Copyright (C) 2006 Stefan Kost <ensonic@users.sf.net>
 * Copyright (C) 2015  <<user@hostname.org>>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/**
 * SECTION:element-qr
 *
 * Draws QR code with frame metadata on the frame
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch -v -m videotestsrc ! qr scale=2,x=10,y=10 ! autovideosink
 * ]|
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <gst/base/base.h>
#include <gst/controller/controller.h>
#include <gst/video/video.h>

#include <qrencode.h>
#include <glib.h>
#include <glib/gprintf.h>
#include <sys/time.h>

#include "gstqr.h"

GST_DEBUG_CATEGORY_STATIC (gst_qr_debug);
#define GST_CAT_DEFAULT gst_qr_debug
#define DEFAULT_SCALE 1
#define DEFAULT_XPOS 10
#define DEFAULT_YPOS 10

/* Filter signals and args */
enum
{
  /* FILL ME */
  LAST_SIGNAL
};

enum
{
  PROP_0,
  PROP_SCALE,
  PROP_XPOS,
  PROP_YPOS,
  PROP_LAST,
};

#define VIDEO_FORMATS "{ I420, YV12, YUY2 } "

static GstStaticPadTemplate sink_template =
GST_STATIC_PAD_TEMPLATE (
  "sink",
  GST_PAD_SINK,
  GST_PAD_ALWAYS,
  GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE (VIDEO_FORMATS))
);

static GstStaticPadTemplate src_template =
GST_STATIC_PAD_TEMPLATE (
  "src",
  GST_PAD_SRC,
  GST_PAD_ALWAYS,
  GST_STATIC_CAPS ("video/x-raw(ANY)")
);

#define gst_qr_parent_class parent_class
G_DEFINE_TYPE (Gstqr, gst_qr, GST_TYPE_BASE_TRANSFORM);

static void gst_qr_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_qr_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static GstFlowReturn gst_qr_transform_ip (GstBaseTransform * base,
    GstBuffer * outbuf);

/* GObject vmethod implementations */

/* initialize the qr's class */
static void
gst_qr_class_init (GstqrClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;

  gobject_class->set_property = gst_qr_set_property;
  gobject_class->get_property = gst_qr_get_property;

  g_object_class_install_property (gobject_class, PROP_SCALE,
      g_param_spec_int ("scale", "Scale",
          "QR Code scaling ratio",
          1, G_MAXUINT8, DEFAULT_SCALE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_XPOS,
      g_param_spec_int ("x", "X-offset",
          "X offset of QR code in frame",
          0, G_MAXINT, DEFAULT_XPOS,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_YPOS,
      g_param_spec_int ("y", "Y-offset",
          "Y offset of QR code in frame",
          0, G_MAXINT, DEFAULT_YPOS,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gst_element_class_set_details_simple (gstelement_class,
    "qr",
    "Generic/Filter",
    "Place QR encoded info on frame",
    "Andrey Panteleyev <<insonifi@gmail.com>>");

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&src_template));
  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&sink_template));

  GST_BASE_TRANSFORM_CLASS (klass)->transform_ip =
      GST_DEBUG_FUNCPTR (gst_qr_transform_ip);

  /* debug category for fltering log messages
   */
  GST_DEBUG_CATEGORY_INIT (gst_qr_debug, "qr", 0, "QR Info");
}

/* initialize the new element
 * initialize instance structure
 */
static void
gst_qr_init (Gstqr *filter)
{
  filter->scale = 1;
  filter->x = 0;
  filter->y = 0;
}

static void
gst_qr_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  Gstqr *filter = GST_QR (object);

  switch (prop_id) {
    case PROP_SCALE:
      filter->scale = g_value_get_int (value);
      break;
    case PROP_XPOS:
      filter->x = g_value_get_int (value);
      break;
    case PROP_YPOS:
      filter->y = g_value_get_int (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_qr_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  Gstqr *filter = GST_QR (object);

  switch (prop_id) {
    case PROP_SCALE:
      g_value_set_int (value, filter->scale);
      break;
    case PROP_XPOS:
      g_value_set_int (value, filter->x);
      break;
    case PROP_YPOS:
      g_value_set_int (value, filter->y);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_qr_render_yuv (Gstqr * render, guchar * pixbuf, QRcode * code,
                   int stride, int pstride)
{
  gint i, j, k, l, m;
  gint xpos = render->x;
  gint ypos = render->y;
  gint fheight = render->height;
  gint s = 2 * render->scale;
  gint border = 3;
  gint r = -1;
  gint8 v;
  guchar *p, *c;
  guchar *data = code->data;
  gint width = code->width;

  gint b = border * s;
  gint bc = (border + width) * s;
  gint bcb = (2 * border + width) * s;

  /* Border Row*/
  for (i = 0; i < b; i++)
  {
    p = pixbuf + (i + ypos) * stride + xpos;
    for (m = 0; m <= bcb; m++)
    {
      *p++ = 255;//Y
      for (l = 1; l < pstride; l++)
        *p++ = 127;//U
    }
  }


  for (i = b; i < bc; i++)
  {
    if ((i % s) == 0)
      r += 1;
    p = pixbuf + (i + ypos) * stride + xpos;
    c = data + r * width;
    /* Border Col */
    for (m = 0; m <= b; m++)
    {
      *p++ = 255;//Y
      for (l = 1; l < pstride; l++)
        *p++ = 127;//U
    }
    /* Code */
    for (j = 0; j < width; j++)
    {
      v = !(*c++ & 1);
      for (k = 0; k < s; k++)
      {
        *p++ = 255 * v;//Y
        for (l = 1; l < pstride; l++)
          *p++ = 127;//U
      }
    }
    /* Border Col */
    for (m = 0; m <= b; m++)
    {
      *p++ = 255;//Y
      for (l = 1; l < pstride; l++)
        *p++ = 127;//U
    }
  }

  /* Border Row */
  for (i = bc; i < bcb; i++)
  {
    p = pixbuf + (i + ypos) * stride + xpos;
    for (m = 0; m <= bcb; m++)
    {
      *p++ = 255;//Y
      for (l = 1; l < pstride; l++)
        *p++ = 127;//U
    }
  }
}


/* GstBaseTransform vmethod implementations */

/* this function does the actual processing
 */
static GstFlowReturn
gst_qr_transform_ip (GstBaseTransform * base, GstBuffer * outbuf)
{
  Gstqr *render = GST_QR (base);
  GstPad *srcpad = GST_BASE_TRANSFORM_SRC_PAD (base);
  GstCaps *caps = gst_pad_get_current_caps (srcpad);
  GstMapInfo map;
  GstStructure *structure;
  GstVideoInfo *vinfo;
  guint8 *data;

  guint64 timestamp = GST_TIME_AS_MSECONDS (outbuf->pts);
  guint64 frameidx = outbuf->offset;

  QRcode *code;
  gchar *qrdata;

  const gchar *format;
  gint version = 0;
  gint case_sensitive = 0;
  gint width, height, num, denom, stride, pstride;
  gfloat fps;

  struct timeval tm;
  gettimeofday (&tm, NULL);

  unsigned long long clock = tm.tv_sec * 1000 + tm.tv_usec;


  vinfo = gst_video_info_new ();
  gst_video_info_from_caps (vinfo, caps);

  stride = GST_VIDEO_INFO_PLANE_STRIDE (vinfo, 0);
  pstride = GST_VIDEO_INFO_COMP_PSTRIDE (vinfo, 0);

  gst_video_info_free (vinfo);

  structure = gst_caps_get_structure (caps, 0);
  format = gst_structure_get_string (structure, "format");

  gst_structure_get_int (structure, "width", &width);
  gst_structure_get_int (structure, "height", &height);
  gst_structure_get_fraction (structure, "framerate", &num, &denom);

  fps = num / denom;

  if (GST_CLOCK_TIME_IS_VALID (GST_BUFFER_TIMESTAMP (outbuf)))
    gst_object_sync_values (GST_OBJECT (render),
                            GST_BUFFER_TIMESTAMP (outbuf));

  qrdata = g_strdup_printf ("clock=%llu\ntimestamp=%" G_GUINT64_FORMAT "\nframe=%"
                            G_GUINT64_FORMAT "\nwidth=%i\nheight=%i\nfps=%.2f",
                            clock, timestamp, frameidx, width, height, fps);
  code = QRcode_encodeString (qrdata, version,
                              QR_ECLEVEL_M, QR_MODE_8, case_sensitive);
  g_free(qrdata);

  gst_buffer_map (outbuf, &map, GST_MAP_WRITE);
  data = map.data;

  gst_qr_render_yuv (render, data, code, stride, pstride);

  QRcode_free(code);
  gst_buffer_unmap (outbuf, &map);

  return GST_FLOW_OK;
}


/* entry point to initialize the plug-in
 * initialize the plug-in itself
 * register the element factories and other features
 */
static gboolean
qr_init (GstPlugin * qr)
{
  return gst_element_register (qr, "qr", GST_RANK_NONE,
      GST_TYPE_QR);
}

/* gstreamer looks for this structure to register qrs
 *
 * FIXME:exchange the string 'Template qr' with you qr description
 */
GST_PLUGIN_DEFINE (
    GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    qr,
    "QR info",
    qr_init,
    VERSION,
    "LGPL",
    "GStreamer",
    "http://gstreamer.net/"
)
