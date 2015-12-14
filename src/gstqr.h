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

#ifndef __GST_QR_H__
#define __GST_QR_H__

#include <gst/gst.h>
#include <gst/base/gstbasetransform.h>

G_BEGIN_DECLS

#define GST_TYPE_QR \
  (gst_qr_get_type())
#define GST_QR(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_QR,Gstqr))
#define GST_QR_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_QR,GstqrClass))
#define GST_IS_QR(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_QR))
#define GST_IS_QR_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_QR))

typedef struct _Gstqr      Gstqr;
typedef struct _GstqrClass GstqrClass;

struct _Gstqr {
  GstBaseTransform element;

  gint x;
  gint y;
  gint width;
  gint height;
  gchar *format;
  gint scale;
  gboolean silent;
};

struct _GstqrClass {
  GstBaseTransformClass parent_class;
};

GType gst_qr_get_type (void);

G_END_DECLS

#endif /* __GST_QR_H__ */
