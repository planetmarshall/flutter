// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <impeller/constants.glsl>
#include <impeller/texture.glsl>
#include <impeller/types.glsl>

uniform f16sampler2D texture_sampler;

layout(constant_id = 0) const float supports_decal = 1.0;

uniform Parameters {
  // X, Y are uv offset and Z is the coefficient normalized by b0. W is padding.
  float B;
  vec4 data[3];
}
parameters;

uniform Bounds {
    float x0;
    float x1;
}
bounds;

f16vec4 Sample(f16sampler2D tex, vec2 coords) {
  if (supports_decal == 1.0) {
    return texture(tex, coords);
  }
  return IPHalfSampleDecal(tex, coords);
}

in vec2 v_texture_coords;

out f16vec4 frag_color;

void main() {

  // horizontal causal pass
  if (v_texture_coords.x < bounds.x0 || v_texture_coords.x >= bounds.x1) {
    frag_color = Sample(texture_sampler, v_texture_coords);
  }
  else {
  frag_color =
      float16_t(parameters.B) * Sample(texture_sampler, v_texture_coords) +
      float16_t(parameters.data[0].z) *
          Sample(texture_sampler, v_texture_coords + parameters.data[0].xy) +
      float16_t(parameters.data[1].z) *
          Sample(texture_sampler, v_texture_coords + parameters.data[1].xy) +
      float16_t(parameters.data[2].z) *
          Sample(texture_sampler, v_texture_coords + parameters.data[2].xy);

  frag_color = vec4(1,0,0,1);
  }
}
