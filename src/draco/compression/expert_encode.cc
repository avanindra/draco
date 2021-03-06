// Copyright 2017 The Draco Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
#include "draco/compression/expert_encode.h"

#include "draco/compression/mesh/mesh_edgebreaker_encoder.h"
#include "draco/compression/mesh/mesh_sequential_encoder.h"
#include "draco/compression/point_cloud/point_cloud_kd_tree_encoder.h"
#include "draco/compression/point_cloud/point_cloud_sequential_encoder.h"

namespace draco {

ExpertEncoder::ExpertEncoder(const PointCloud &point_cloud)
    : point_cloud_(&point_cloud), mesh_(nullptr) {}

ExpertEncoder::ExpertEncoder(const Mesh &mesh)
    : point_cloud_(&mesh), mesh_(&mesh) {}

Status ExpertEncoder::EncodeToBuffer(EncoderBuffer *out_buffer) {
  if (point_cloud_ == nullptr)
    return Status(Status::ERROR, "Invalid input geometry.");
  if (mesh_ == nullptr) {
    return EncodePointCloudToBuffer(*point_cloud_, out_buffer);
  }
  return EncodeMeshToBuffer(*mesh_, out_buffer);
}

Status ExpertEncoder::EncodePointCloudToBuffer(const PointCloud &pc,
                                               EncoderBuffer *out_buffer) {
  std::unique_ptr<PointCloudEncoder> encoder;
  const int encoding_method = options().GetGlobalInt("encoding_method", -1);
  if (encoding_method == POINT_CLOUD_KD_TREE_ENCODING ||
      (options().GetSpeed() < 10 && pc.num_attributes() == 1)) {
    const PointAttribute *const att = pc.attribute(0);
    bool create_kd_tree_encoder = true;
    // Kd-Tree encoder can be currently used only under following conditions:
    //   - Point cloud has one attribute describing positions
    //   - Position is described by three components (x,y,z)
    //   - Position data type is one of the following:
    //         -float32 and quantization is enabled
    //         -uint32
    if (att->attribute_type() != GeometryAttribute::POSITION ||
        att->num_components() != 3)
      create_kd_tree_encoder = false;
    if (create_kd_tree_encoder && att->data_type() != DT_FLOAT32 &&
        att->data_type() != DT_UINT32)
      create_kd_tree_encoder = false;
    if (create_kd_tree_encoder && att->data_type() == DT_FLOAT32 &&
        options().GetAttributeInt(0, "quantization_bits", -1) <= 0)
      create_kd_tree_encoder = false;  // Quantization not enabled.
    if (create_kd_tree_encoder) {
      // Create kD-tree encoder (all checks passed).
      encoder =
          std::unique_ptr<PointCloudEncoder>(new PointCloudKdTreeEncoder());
    } else if (encoding_method == POINT_CLOUD_KD_TREE_ENCODING) {
      // Encoding method was explicitly specified but we cannot use it for the
      // given input (some of the checks above failed).
      return Status(Status::ERROR, "Invalid encoding method.");
    }
  }
  if (!encoder) {
    // Default choice.
    encoder =
        std::unique_ptr<PointCloudEncoder>(new PointCloudSequentialEncoder());
  }
  encoder->SetPointCloud(pc);
  return encoder->Encode(options(), out_buffer);
}

Status ExpertEncoder::EncodeMeshToBuffer(const Mesh &m,
                                         EncoderBuffer *out_buffer) {
  std::unique_ptr<MeshEncoder> encoder;
  // Select the encoding method only based on the provided options.
  int encoding_method = options().GetGlobalInt("encoding_method", -1);
  if (encoding_method == -1) {
    // For now select the edgebreaker for all options expect of speed 10
    if (options().GetSpeed() == 10) {
      encoding_method = MESH_SEQUENTIAL_ENCODING;
    } else {
      encoding_method = MESH_EDGEBREAKER_ENCODING;
    }
  }
  if (encoding_method == MESH_EDGEBREAKER_ENCODING) {
    encoder = std::unique_ptr<MeshEncoder>(new MeshEdgeBreakerEncoder());
  } else {
    encoder = std::unique_ptr<MeshEncoder>(new MeshSequentialEncoder());
  }
  encoder->SetMesh(m);
  return encoder->Encode(options(), out_buffer);
}

void ExpertEncoder::Reset(const EncoderOptions &options) {
  Base::Reset(options);
}

void ExpertEncoder::Reset() { Base::Reset(); }

void ExpertEncoder::SetSpeedOptions(int encoding_speed, int decoding_speed) {
  Base::SetSpeedOptions(encoding_speed, decoding_speed);
}

void ExpertEncoder::SetAttributeQuantization(int32_t attribute_id,
                                             int quantization_bits) {
  options().SetAttributeInt(attribute_id, "quantization_bits",
                            quantization_bits);
}

void ExpertEncoder::SetUseBuiltInAttributeCompression(bool enabled) {
  options().SetGlobalBool("use_built_in_attribute_compression", enabled);
}

void ExpertEncoder::SetEncodingMethod(int encoding_method) {
  Base::SetEncodingMethod(encoding_method);
}

void ExpertEncoder::SetAttributePredictionScheme(int32_t attribute_id,
                                                 int prediction_scheme_method) {
  options().SetAttributeInt(attribute_id, "prediction_scheme",
                            prediction_scheme_method);
}

}  // namespace draco
