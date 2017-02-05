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
 * gst-launch videotestsrc ! qr scale=2 x=10 y=10 format="clock=%c\nts=%t" ! autovideosink
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
#define DEFAULT_FORMAT "%t"
#define DEFAULT_BORDER 2
#define QR_VERSION_0 0
#define QR_CASE_SENSITIVE 1

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
  PROP_FORMAT,
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
G_DEFINE_TYPE (GstQr, gst_qr, GST_TYPE_BASE_TRANSFORM);

static void gst_qr_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_qr_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static GstFlowReturn gst_qr_transform_ip (GstBaseTransform * base,
    GstBuffer * outbuf);

/* GObject vmethod implementations */

/* initialize the qr's class */
static void
gst_qr_class_init (GstQrClass * klass)
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

  g_object_class_install_property (gobject_class, PROP_FORMAT,
      g_param_spec_string ("format", "Coded string format",
          "\n%c - System clock (µs)\n"
          "%t - Timestamp (µs)\n"
          "%n - Frame number\n"
          "%f - Video format\n"
          "%w - Frame width\n"
          "%h - Frame height\n"
          "%r - Framerate",
          DEFAULT_FORMAT,
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
gst_qr_init (GstQr *filter)
{
  filter->scale = DEFAULT_SCALE;
  filter->x = DEFAULT_XPOS; 
  filter->y = DEFAULT_YPOS;
  filter->format = DEFAULT_FORMAT;
  filter->border = DEFAULT_BORDER;
}

static void
gst_qr_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstQr *filter = GST_QR (object);

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
    case PROP_FORMAT:
      filter->format = g_strdup (g_value_get_string (value));
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
  GstQr *filter = GST_QR (object);

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
    case PROP_FORMAT:
      g_value_set_string (value, filter->format);
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
gst_qr_render_yuv (GstQr * render, GstVideoFrame * frame, QRcode * code)
{
  gint i, j, k, l;
  gint s = render->scale;
  gint x = render->x;
  gint y = render->y;
  gint border = render->border;
  gint comp = 0;
  gpointer data;
  guchar *p, *code_val;
  guchar *qr = code->data;
  gint qrwidth = code->width;
  gint fwidth = GST_VIDEO_FRAME_WIDTH(frame);
  gint fheight = GST_VIDEO_FRAME_HEIGHT(frame);
  gboolean is_white;

  guint size = (2 * border + qrwidth);
  guint w = size * s;
  guint b, b_w, b_h;
  guint w_w, w_h;
  guint xpos, ypos;
  guint qr_i, qr_j;
  guint row = 0;

  guint stride, pstride, val, w_sub, h_sub;
  guint n_comp = GST_VIDEO_FRAME_N_COMPONENTS (frame);
  gboolean value[GST_VIDEO_MAX_COMPONENTS];
  const GstVideoInfo *vinfo = &(frame->info);
  const GstVideoFormatInfo *finfo = vinfo->finfo;

  switch (GST_VIDEO_INFO_FLAGS (finfo)) {
    case GST_VIDEO_FORMAT_FLAG_GRAY:
    case GST_VIDEO_FORMAT_FLAG_RGB:
      for (comp = 0; comp < n_comp; comp++)
      {
        value[comp] = TRUE;
      }
      break;
    case GST_VIDEO_FORMAT_FLAG_YUV:
      value[comp] = TRUE;
      for (comp = 1; comp < n_comp; comp++)
      {
        value[comp] = FALSE;
      }
      break;
    default:
      break;
  }

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
  /* Fit position to frame */  
  x = (x + w) < fwidth ? x :  fwidth - w; 
  y = (y + w) < fheight ? y :  fheight - w; 

  for (comp = 0; comp < n_comp; comp++)
  {
    stride = GST_VIDEO_FRAME_COMP_STRIDE (frame, comp);
    pstride = GST_VIDEO_FRAME_COMP_PSTRIDE (frame, comp);
    data = GST_VIDEO_FRAME_COMP_DATA (frame, comp);
    val = (1 << GST_VIDEO_FRAME_COMP_DEPTH (frame, comp)) - 1;
    w_sub = GST_VIDEO_FORMAT_INFO_W_SUB(finfo, comp);
    h_sub = GST_VIDEO_FORMAT_INFO_H_SUB(finfo, comp);

    /* Get values scaled to format */
    b_w = GST_VIDEO_SUB_SCALE (w_sub, b);
    b_h = GST_VIDEO_SUB_SCALE (h_sub, b);
    w_w = GST_VIDEO_SUB_SCALE (w_sub, w);
    w_h = GST_VIDEO_SUB_SCALE (h_sub, w);
    xpos = GST_VIDEO_SUB_SCALE (w_sub, x);
    ypos = GST_VIDEO_SUB_SCALE (h_sub, y);

    if (!value[comp]) {
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

    for (qr_i = 0; qr_i < qrwidth && value[comp]; qr_i++)
    {
      for (l = 0; l < s; l++)
      {
        p = data + (qr_i + (ypos + b_h + row++)) * stride +
                            (xpos + b_w) * pstride;
        code_val = qr + qr_i * qrwidth;
        /* Code */
        for (qr_j = 0; qr_j < qrwidth; qr_j++)
        {
          /* Get QR code pixel value */
          is_white = !(*code_val++ & 1);
          /* draw single pixel of QR code */
          for (k = 0; k < s; k++)
          {
            *p = val * is_white;
            p += pstride;
          }
        }
      }
      row--;
    }
  }
}

static gchar *
format_buffer_info (GstBuffer * buf, GstVideoInfo *vinfo, const gchar * format)
{
  const gchar *vformat = GST_VIDEO_INFO_NAME (vinfo);
  guint64 width = GST_VIDEO_INFO_WIDTH (vinfo);
  guint64 height = GST_VIDEO_INFO_HEIGHT (vinfo);
  guint64 num = GST_VIDEO_INFO_FPS_N (vinfo);
  guint64 denom = GST_VIDEO_INFO_FPS_D (vinfo);
  guint64 frame_n = GST_BUFFER_OFFSET (buf);
  guint64 pts_us = GST_TIME_AS_USECONDS (GST_BUFFER_PTS (buf));

  GString *output = g_string_new(NULL);
  const gchar *i = format;

  while (*i)
  {
    if (*i == '%')
    {
      switch (*(i + 1))
      {
        case 'c':
          g_string_append_printf (output, "%" G_GUINT64_FORMAT, g_get_real_time ());
          i += 2;
          break;
        case 't':
          g_string_append_printf (output, "%" G_GUINT64_FORMAT, pts_us);
          i += 2;
          break;
        case 'n':
          g_string_append_printf (output, "%" G_GUINT64_FORMAT, frame_n);
          i += 2;
          break;
        case 'f':
          g_string_append (output, vformat);
          i += 2;
          break;
        case 'w':
          g_string_append_printf (output, "%" G_GUINT64_FORMAT, width);
          i += 2;
          break;
        case 'h':
          g_string_append_printf (output, "%" G_GUINT64_FORMAT, height);
          i += 2;
          break;
        case 'r':
        {
          gfloat fps = num / denom;
          g_string_append_printf (output, "%f.2", fps);
          i += 2;
          break;
        }
      }
    } else {
      g_string_append_len (output, i++, 1);
    }
  } 
  return g_string_free (output, FALSE);
}

/* GstBaseTransform vmethod implementations */

/* this function does the actual processing
 */
static GstFlowReturn
gst_qr_transform_ip (GstBaseTransform * base, GstBuffer * outbuf)
{
  GstQr *render = GST_QR (base);
  GstPad *srcpad = GST_BASE_TRANSFORM_SRC_PAD (base);
  GstCaps *caps = gst_pad_get_current_caps (srcpad);
  GstVideoInfo *vinfo;
  GstVideoFrame frame;

  QRcode *code;
  gchar *qrdata;

  vinfo = gst_video_info_new ();
  gst_video_info_from_caps (vinfo, caps);

  if (!gst_video_frame_map (&frame, vinfo, outbuf, GST_MAP_READWRITE))
    goto invalid_frame;

  if (GST_CLOCK_TIME_IS_VALID (GST_BUFFER_TIMESTAMP (outbuf)))
    gst_object_sync_values (GST_OBJECT (render),
                            GST_BUFFER_TIMESTAMP (outbuf));

  qrdata = format_buffer_info (outbuf, vinfo, render->format);
  code = QRcode_encodeString (qrdata, QR_VERSION_0,
                              QR_ECLEVEL_M, QR_MODE_8, QR_CASE_SENSITIVE);

  if (code == NULL) {
    goto invalid_data;
  }

  gst_qr_render_yuv (render, &frame, code);

invalid_data:
  gst_video_info_free (vinfo);
  g_free (qrdata);
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
