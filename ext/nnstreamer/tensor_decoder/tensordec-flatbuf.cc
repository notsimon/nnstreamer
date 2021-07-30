/* SPDX-License-Identifier: LGPL-2.1-only */
/**
 * GStreamer / NNStreamer tensor_decoder subplugin, "Flatbuffer"
 * Copyright (C) 2020 Gichan Jang <gichan2.jang@samsung.com>
 */
/**
 * @file        tensordec-flatbuf.cc
 * @date        26 Feb 2020
 * @brief       NNStreamer tensor-decoder subplugin, "flatbuffer",
 *              which converts tensor or tensors to flatbuffer byte stream.
 *
 * @see         https://github.com/nnstreamer/nnstreamer
 * @author      Gichan Jang <gichan2.jang@samsung.com>
 * @bug         No known bugs except for NYI items
 *
 */

#include <glib.h>
#include <gst/gstinfo.h>
#include <iostream>
#include <nnstreamer_generated.h> /* Generated by `flatc`. */
#include <nnstreamer_log.h>
#include <nnstreamer_plugin_api.h>
#include <nnstreamer_plugin_api_decoder.h>
#include <nnstreamer_util.h>
#include <typeinfo>
#include "tensordecutil.h"

namespace nnstreamer
{
namespace flatbuf
{

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */
void init_fbd (void) __attribute__ ((constructor));
void fini_fbd (void) __attribute__ ((destructor));
#ifdef __cplusplus
}
#endif /* __cplusplus */

/** @brief tensordec-plugin's GstTensorDecoderDef callback */
static int
fbd_init (void **pdata)
{
  *pdata = NULL;
  return TRUE;
}

/** @brief tensordec-plugin's GstTensorDecoderDef callback */
static void
fbd_exit (void **pdata)
{
  UNUSED (pdata);
  return;
}

/** @brief tensordec-plugin's GstTensorDecoderDef callback */
static int
fbd_setOption (void **pdata, int opNum, const char *param)
{
  UNUSED (pdata);
  UNUSED (opNum);
  UNUSED (param);
  return TRUE;
}

/** @brief tensordec-plugin's GstTensorDecoderDef callback */
static GstCaps *
fbd_getOutCaps (void **pdata, const GstTensorsConfig *config)
{
  GstCaps *caps;
  UNUSED (pdata);

  caps = gst_caps_from_string (GST_FLATBUF_TENSOR_CAP_DEFAULT);
  setFramerateFromConfig (caps, config);
  return caps;
}

/** @brief tensordec-plugin's GstTensorDecoderDef callback */
static GstFlowReturn
fbd_decode (void **pdata, const GstTensorsConfig *config,
    const GstTensorMemory *input, GstBuffer *outbuf)
{
  char *name;
  Tensor_type type;
  GstMapInfo out_info;
  GstMemory *out_mem;
  guint i, num_tensors;
  flatbuffers::uoffset_t fb_size;
  flatbuffers::FlatBufferBuilder builder;
  std::vector<flatbuffers::Offset<Tensor>> tensor_vector;
  flatbuffers::Offset<flatbuffers::Vector<uint32_t>> dim;
  flatbuffers::Offset<flatbuffers::String> tensor_name;
  flatbuffers::Offset<flatbuffers::Vector<unsigned char>> input_vector;
  flatbuffers::Offset<Tensor> tensor;
  flatbuffers::Offset<Tensors> tensors;
  frame_rate fr;
  UNUSED (pdata);

  if (!config || !input || !outbuf) {
    ml_loge ("NULL parameter is passed to tensor_decoder::flatbuf");
    return GST_FLOW_ERROR;
  }

  num_tensors = config->info.num_tensors;
  fr = frame_rate (config->rate_n, config->rate_d);
  /* Fill the info in tensor and puth to tensor vector */
  for (i = 0; i < num_tensors; i++) {
    unsigned char *tmp_buf;

    dim = builder.CreateVector (config->info.info[i].dimension, NNS_TENSOR_RANK_LIMIT);
    name = config->info.info[i].name;

    if (name == NULL)
      tensor_name = builder.CreateString ("");
    else
      tensor_name = builder.CreateString (name);

    type = (Tensor_type)config->info.info[i].type;

    /* Create the vector first, and fill in data later */
    /** @todo Consider to remove memcpy */
    input_vector = builder.CreateUninitializedVector<unsigned char> (input[i].size, &tmp_buf);
    memcpy (tmp_buf, input[i].data, input[i].size);

    tensor = CreateTensor (builder, tensor_name, type, dim, input_vector);
    tensor_vector.push_back (tensor);
  }

  tensors = CreateTensors (builder, num_tensors, &fr, builder.CreateVector (tensor_vector));

  /* Serialize the data.*/
  builder.Finish (tensors);
  fb_size = builder.GetSize ();

  if (gst_buffer_get_size (outbuf) == 0) {
    out_mem = gst_allocator_alloc (NULL, fb_size, NULL);
  } else {
    if (gst_buffer_get_size (outbuf) < fb_size) {
      gst_buffer_set_size (outbuf, fb_size);
    }
    out_mem = gst_buffer_get_all_memory (outbuf);
  }

  if (!gst_memory_map (out_mem, &out_info, GST_MAP_WRITE)) {
    gst_memory_unref (out_mem);
    nns_loge ("Cannot map gst memory (tensor decoder flatbuf)\n");
    return GST_FLOW_ERROR;
  }

  memcpy (out_info.data, builder.GetBufferPointer (), fb_size);

  gst_memory_unmap (out_mem, &out_info);

  if (gst_buffer_get_size (outbuf) == 0)
    gst_buffer_append_memory (outbuf, out_mem);
  else
    gst_memory_unref (out_mem);

  return GST_FLOW_OK;
}

static gchar decoder_subplugin_flatbuf[] = "flatbuf";

/** @brief flatbuffer tensordec-plugin GstTensorDecoderDef instance */
static GstTensorDecoderDef flatBuf = { .modename = decoder_subplugin_flatbuf,
  .init = fbd_init,
  .exit = fbd_exit,
  .setOption = fbd_setOption,
  .getOutCaps = fbd_getOutCaps,
  .decode = fbd_decode,
  .getTransformSize = NULL };

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/** @brief Initialize this object for tensordec-plugin */
void
init_fbd (void)
{
  nnstreamer_decoder_probe (&flatBuf);
}

/** @brief Destruct this object for tensordec-plugin */
void
fini_fbd (void)
{
  nnstreamer_decoder_exit (flatBuf.modename);
}
#ifdef __cplusplus
}
#endif /* __cplusplus */

}; /* Namespace flatbuf */
}; /* Namespace nnstreamer */
