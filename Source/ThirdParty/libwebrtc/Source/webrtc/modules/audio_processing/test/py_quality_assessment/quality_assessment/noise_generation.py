# Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

class NoiseGenerator(object):

  NAME = None
  REGISTERED_CLASSES = {}

  def __init__(self):
    pass

  @classmethod
  def register_class(cls, class_to_register):
    """
    Decorator to automatically register the classes that extend NoiseGenerator.
    """
    cls.REGISTERED_CLASSES[class_to_register.NAME] = class_to_register


# Identity generator.
@NoiseGenerator.register_class
class IdentityGenerator(NoiseGenerator):
  """
  Generator that adds no noise, therefore both the noisy and the reference
  signals are the input signal.
  """

  NAME = 'identity'

  def __init__(self):
    NoiseGenerator.__init__(self)


@NoiseGenerator.register_class
class WhiteNoiseGenerator(NoiseGenerator):
  """
  Additive white noise generator.
  """

  NAME = 'white'

  def __init__(self):
    NoiseGenerator.__init__(self)


@NoiseGenerator.register_class
class NarrowBandNoiseGenerator(NoiseGenerator):
  """
  Additive narrow-band noise generator.
  """

  NAME = 'narrow_band'

  def __init__(self):
    NoiseGenerator.__init__(self)


@NoiseGenerator.register_class
class EnvironmentalNoiseGenerator(NoiseGenerator):
  """
  Additive environmental noise generator.
  """

  NAME = 'environmental'

  def __init__(self):
    NoiseGenerator.__init__(self)


@NoiseGenerator.register_class
class EchoNoiseGenerator(NoiseGenerator):
  """
  Echo noise generator.
  """

  NAME = 'echo'

  def __init__(self):
    NoiseGenerator.__init__(self)
