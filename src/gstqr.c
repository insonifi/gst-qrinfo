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
 * gst-launch -v -m videotestsrc ! qr scale=2 x=10 y=10 string="123" ! autovideosink
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
#include <sys/time.h>

#include "gstqr.h"

GST_DEBUG_CATEGORY_STATIC (gst_qr_debug);
#define GST_CAT_DEFAULT gst_qr_debug
#define DEFAULT_SCALE 1
#define DEFAULT_XPOS 10
#define DEFAULT_YPOS 10
#define DEFAULT_EMPTY_STR ""
#define DEFAULT_BORDER 2

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
  PROP_STRING,
  PROP_BORDER,
  PROP_LAST,
};

static GstStaticPadTemplate sink_template =
GST_STATIC_PAD_TEMPLATE (
  "sink",
  GST_PAD_SINK,
  GST_PAD_ALWAYS,
  GST_STATIC_CAPS ("video/x-raw(ANY)")
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

  g_object_class_install_property (gobject_class, PROP_STRING,
      g_param_spec_string ("string", "String to add",
          "String added to QR code data",
          DEFAULT_EMPTY_STR,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_BORDER,
      g_param_spec_int ("border", "Border width",
          "Width of border around QR code",
          0, G_MAXINT, DEFAULT_BORDER,
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
  filter->scale = DEFAULT_SCALE;
  filter->x = DEFAULT_XPOS; 
  filter->y = DEFAULT_YPOS;
  filter->string = DEFAULT_EMPTY_STR;
  filter->border = DEFAULT_BORDER;
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
    case PROP_STRING:
      filter->string = g_strdup( g_value_get_string (value) );
      break;
    case PROP_BORDER:
      filter->border = g_value_get_int (value);
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
    case PROP_STRING:
      g_value_set_string (value, filter->string);
      break;
    case PROP_BORDER:
      g_value_set_int (value, filter->border);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_qr_render_yuv (Gstqr * render, GstVideoFrame * frame, QRcode * code)
{
  gint i, j, k;
  gint s = render->scale;
  gint x = render->x;
  gint y = render->y;
  gint border = render->border;
  gint cm = 0;
  gint r = -1;
  gpointer data;
  guchar *p, *c;
  guchar *qr = code->data;
  gint qrwidth = code->width;
  gint fwidth = GST_VIDEO_FRAME_WIDTH(frame);
  gint fheight = GST_VIDEO_FRAME_HEIGHT(frame);
  gboolean v;

  guint size = (2 * border + qrwidth);
  guint w = size * s;
  guint b, b_w, b_h;
  guint cw, cw_h;
  guint w_w, w_h;
  guint xpos, ypos;
  guint qr_w;

  guint stride, pstride, val, w_sub, h_sub;
  guint n_comp = GST_VIDEO_FRAME_N_COMPONENTS (frame);
  gboolean value[GST_VIDEO_MAX_COMPONENTS];
  const GstVideoInfo *vinfo = &(frame->info);
  const GstVideoFormatInfo *finfo = vinfo->finfo;
  /* Fit scaling factor to frame */
  if (w > fwidth) {
    s = fwidth / size; 
    w = size * s;
  }
  if (w > fheight) {
    s = fheight / size;
    w = size * s;
  }
  b = border * s;
  cw = qrwidth * s;
  /* Fit position to frame */  
  x = (x + w) < fwidth ? x :  fwidth - w; 
  y = (y + w) < fheight ? y :  fheight - w; 

  switch (GST_VIDEO_INFO_FLAGS (finfo)) {
    case GST_VIDEO_FORMAT_FLAG_GRAY:
    case GST_VIDEO_FORMAT_FLAG_RGB:
      for (cm = 0; cm < n_comp; cm++)
      {
        value[cm] = TRUE;
      }
      break;
    case GST_VIDEO_FORMAT_FLAG_YUV:
      value[cm] = TRUE;
      for (cm = 1; cm < n_comp; cm++)
      {
        value[cm] = FALSE;
      }
      break;
    default:
      break;
  }

  for (cm = 0; cm < n_comp; cm++)
  {
    stride = GST_VIDEO_FRAME_COMP_STRIDE (frame, cm);
    pstride = GST_VIDEO_FRAME_COMP_PSTRIDE (frame, cm);
    data = GST_VIDEO_FRAME_COMP_DATA (frame, cm);
    val = (1 << GST_VIDEO_FRAME_COMP_DEPTH (frame, cm)) - 1;
    w_sub = GST_VIDEO_FORMAT_INFO_W_SUB(finfo, cm);
    h_sub = GST_VIDEO_FORMAT_INFO_H_SUB(finfo, cm);

    /* Get values scaled to format */
    b_w = GST_VIDEO_SUB_SCALE (w_sub, b);
    b_h = GST_VIDEO_SUB_SCALE (h_sub, b);
    cw_h = GST_VIDEO_SUB_SCALE (h_sub, cw);
    w_w = GST_VIDEO_SUB_SCALE (w_sub, w);
    w_h = GST_VIDEO_SUB_SCALE (h_sub, w);
    qr_w = GST_VIDEO_SUB_SCALE (w_sub, qrwidth);
    xpos = GST_VIDEO_SUB_SCALE (w_sub, x);
    ypos = GST_VIDEO_SUB_SCALE (h_sub, y);

    if (!value[cm]) {
      val = val / 2;
    }
    
    /* Fill QR Code background */
    for (i = 0; i < w_h; i++)
    {
      p = data + (i + ypos) * stride +
                      xpos * pstride;
      for (j = 0; j < w_w; j++)
      {
        *p = val;
        p += pstride;
      }
    }

    for (i = 0; i < cw_h; i++)
    {
      if ((i % s) == 0)
        r += 1;
      p = data + (i + (ypos + b_h)) * stride +
                      (xpos + b_w) * pstride;
      c = qr + r * qrwidth;
      /* Code */
      for (j = 0; j < qr_w; j++)
      {
        v = !(*c++ & 1) || !value[cm];
        for (k = 0; k < s; k++)
        {
          *p = val * v;
          p += pstride;
        }
      }
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
  GstStructure *structure;
  GstVideoInfo *vinfo;
  GstVideoFrame frame;

  guint64 timestamp = GST_TIME_AS_MSECONDS (outbuf->pts);
  guint64 frameidx = outbuf->offset;

  QRcode *code;
  gchar *qrdata;

  const gchar *format;
  gint version = 0;
  gint case_sensitive = 1;
  gint width, height, num, denom; 
  gfloat fps;

  struct timeval tm;
  gettimeofday (&tm, NULL);

  unsigned long long clock = tm.tv_sec * 1000 + tm.tv_usec;


  vinfo = gst_video_info_new ();
  gst_video_info_from_caps (vinfo, caps);

  if (!gst_video_frame_map (&frame, vinfo, outbuf, GST_MAP_READWRITE))
    goto invalid_frame;
  structure = gst_caps_get_structure (caps, 0);
  format = gst_structure_get_string (structure, "format");

  gst_structure_get_int (structure, "width", &width);
  gst_structure_get_int (structure, "height", &height);
  gst_structure_get_fraction (structure, "framerate", &num, &denom);

  fps = num / denom;

  if (GST_CLOCK_TIME_IS_VALID (GST_BUFFER_TIMESTAMP (outbuf)))
    gst_object_sync_values (GST_OBJECT (render),
                            GST_BUFFER_TIMESTAMP (outbuf));

  qrdata = g_strdup_printf ("clock=%llu\n"
                            "timestamp=%" G_GUINT64_FORMAT "\n"
                            "frame=%" G_GUINT64_FORMAT "\n"
                            "width=%d\n"
                            "height=%d\n"
                            "fps=%.2f\n"
                            "format=%s\n"
                            "%s",
                            clock, timestamp, frameidx,
                            width, height, fps, format, render->string);
  code = QRcode_encodeString (qrdata, version,
                              QR_ECLEVEL_M, QR_MODE_8, case_sensitive);
  

  gst_qr_render_yuv (render, &frame, code);

  gst_video_info_free (vinfo);
  g_free(qrdata);
  QRcode_free(code);
  gst_video_frame_unmap (&frame);
invalid_frame:
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
