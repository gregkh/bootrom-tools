#! /usr/bin/python

#
# Copyright (c) 2015 Google Inc.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
# 1. Redistributions of source code must retain the above copyright notice,
# this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright notice,
# this list of conditions and the following disclaimer in the documentation
# and/or other materials provided with the distribution.
# 3. Neither the name of the copyright holder nor the names of its
# contributors may be used to endorse or promote products derived from this
# software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
# THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
# OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
# OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
# ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#

from __future__ import print_function
from string import rfind
from struct import unpack_from
from ffff_element import FFFF_MAX_HEADER_BLOCK_OFFSET, FFFF_SENTINEL, \
    FFFF_HDR_OFF_TAIL_SENTINEL, FFFF_HDR_LEN_TAIL_SENTINEL, \
    FFFF_FILE_EXTENSION, FFFF_HDR_VALID, \
    FFFF_HEADER_SIZE_MIN, FFFF_HEADER_SIZE_MAX, FFFF_HEADER_SIZE_DEFAULT, \
    FFFF_HDR_LEN_FIXED_PART, FFFF_ELT_LENGTH, \
    FFFF_RSVD_SIZE, FFFF_HDR_NUM_RESERVED, FFFF_HDR_OFF_RESERVED, \
    FFFF_HDR_LEN_RESERVED
from ffff import Ffff, get_header_block_size
from util import is_power_of_2
import io


# FFFF ROMimage representation (2x FFFF headers + Nx TFTF blobs)
#
class FfffRomimage:

    # FFFF constructor
    #
    def __init__(self):
        """FFFF ROMimage constructor

        The FFFF representation contains a buffer of the ROM span covered by
        the image.
        """
        # FFFF header fields
        self.header_size = FFFF_HEADER_SIZE_DEFAULT
        self.ffff0 = None
        self.ffff1 = None
        self.ffff_buf = None
        self.mv = None
        self.flash_image_name = None
        self.flash_capacity = 0
        self.erase_block_size = 0
        self.flash_image_length = 0
        self.header_generation_number = 0
        self.element_location_min = 0
        self.element_location_max = 0

    def init(self, flash_image_name, flash_capacity, erase_block_size,
             image_length, header_generation_number, header_size):
        """"FFFF post-constructor initializer for a new FFFF

        FFFF post-constructor initializer for creating an FFFF (as opposed
        to reading an existing one from a file), and returns a success flag.
        The FFFF ROMimage buffer is sized explicitly from the image_length
        parameter.
        """
        # Validate the parameters
        if (header_size < FFFF_HEADER_SIZE_MIN) or \
           (header_size > FFFF_HEADER_SIZE_MAX):
            raise ValueError("header_size is out of range")
        elif not is_power_of_2(erase_block_size):
            raise ValueError("Erase block size must be 2**n")
        elif (image_length % erase_block_size) != 0:
            raise ValueError("Image length must be a multiple "
                             "of erase bock size")

        self.header_size = header_size
        self.flash_image_name = flash_image_name
        self.flash_capacity = flash_capacity
        self.erase_block_size = erase_block_size
        self.flash_image_length = image_length
        self.header_generation_number = header_generation_number

        # Determine the ROM range that can hold the elements
        self.element_location_min = 2 * self.get_header_block_size()
        self.element_location_max = image_length

        # Resize the ROMimage buffer to the correct size
        self.ffff_buf = bytearray(image_length)
        #self.mv = memoryview(self.ffff_buf)

        # Create the 2 FFFF headers
        self.ffff0 = Ffff(self.ffff_buf,
                          0,
                          flash_image_name,
                          flash_capacity,
                          erase_block_size,
                          image_length,
                          header_generation_number,
                          header_size)
        self.ffff1 = Ffff(self.ffff_buf,
                          self.get_header_block_size(),
                          flash_image_name,
                          flash_capacity,
                          erase_block_size,
                          image_length,
                          header_generation_number,
                          header_size)
        return True

    def init_from_file(self, filename):
        """"FFFF post-constructor initializer to read an FFFF from file

        Distinct from "init" above, this reads in an existing FFFF file
        and parses it, returning a success flag. The FFFF ROMimage buffer
        is sized to the supplied file.
        """
        if filename:
            # Try to open the file, and if that fails, try appending the
            # extension.
            names = (filename, filename + FFFF_FILE_EXTENSION)
            rf = None
            for i in range(len(names)):
                try:
                    rf = io.open(names[i], 'rb')
                    break
                except:
                    rf = None

            if not rf:
                raise IOError(" can't find FFFF file" + filename)

            try:
                # Read the FFFF file.
                rf.seek(0, 2)
                read_size = rf.tell()

                # Resize the buffer to hold the file
                self.ffff_buf = bytearray(read_size)
                rf.seek(0, 0)
                rf.readinto(self.ffff_buf)
                rf.close()
            except IOError:
                raise IOError("can't read {0:s}".format(filename))

            self.get_romimage_characteristics()

            # Create the 1st FFFF header/object
            #self.mv = memoryview(self.ffff_buf)
            self.ffff0 = Ffff(self.ffff_buf, 0,
                              self.flash_image_name,
                              self.flash_capacity,
                              self.erase_block_size,
                              self.flash_image_length,
                              self.header_generation_number,
                              0)
            self.ffff0.unpack()

            # Scan for 2nd header
            offset = self.get_header_block_size()
            while offset < FFFF_MAX_HEADER_BLOCK_OFFSET:
                # Unpack and validate the nose and tail sentinels
                ffff_hdr = unpack_from("<16s", self.ffff_buf,
                                       offset)
                nose_sentinel = ffff_hdr[0]
                ffff_hdr = unpack_from("<16s", self.ffff_buf,
                                       offset +
                                       FFFF_HDR_OFF_TAIL_SENTINEL)
                tail_sentinel = ffff_hdr[0]

                # Create the 2nd FFFF header/object?
                if nose_sentinel == FFFF_SENTINEL and \
                        tail_sentinel == FFFF_SENTINEL:
                    self.ffff1 = Ffff(self.ffff_buf, offset,
                                      self.flash_image_name,
                                      self.flash_capacity,
                                      self.erase_block_size,
                                      self.flash_image_length,
                                      self.header_generation_number,
                                      0)
                    self.ffff1.unpack()
                    break
                else:
                    offset <<= 1
        else:
            raise ValueError("no file specified")

        return True

    def get_header_block_size(self):
        # Determine the size of the FFFF header block, defined as a
        # power-of-2 * the erase-block-size
        return get_header_block_size(self.erase_block_size, self.header_size)

    def recalculate_header_offsets(self):
        """ Recalculate element table size and offsets from header_size

        Because we have variable-size FFFF headers, we need to recalculate the
        number of entries in the section table, and the offsets to all fields
        which follow.
        """
        global FFFF_HDR_NUM_ELEMENTS, FFFF_HDR_LEN_ELEMENT_TBL, \
            FFFF_HDR_NUM_RESERVED, FFFF_HDR_LEN_RESERVED, \
            FFFF_HDR_OFF_RESERVED, FFFF_RSVD_SIZE, FFFF_HDR_OFF_TAIL_SENTINEL
        # TFTF section table and derived lengths
        FFFF_HDR_NUM_ELEMENTS = \
            ((self.header_size - FFFF_HDR_LEN_FIXED_PART) // FFFF_ELT_LENGTH)
        FFFF_HDR_LEN_ELEMENT_TBL = (FFFF_HDR_NUM_ELEMENTS * FFFF_ELT_LENGTH)

        self.reserved = [0] * FFFF_HDR_NUM_RESERVED

        # Offsets to fields following the section table
        FFFF_HDR_OFF_ELEMENT_TBL = (FFFF_HDR_OFF_RESERVED +
                                    FFFF_HDR_LEN_RESERVED)
        FFFF_HDR_OFF_TAIL_SENTINEL = (self.header_size -
                                      FFFF_HDR_LEN_TAIL_SENTINEL)

    def get_romimage_characteristics(self):
        # Extract the ROMimage size and characteritics from the first FFFF
        # header in the buffer.

        # Unpack the fixed part of the header
        ffff_hdr = unpack_from("<16s16s48sLLLLL", self.ffff_buf)
        sentinel = ffff_hdr[0]
        self.timestamp = ffff_hdr[1]
        self.flash_image_name = ffff_hdr[2]
        self.flash_capacity = ffff_hdr[3]
        self.erase_block_size = ffff_hdr[4]
        self.header_size = ffff_hdr[5]
        self.flash_image_length = ffff_hdr[6]
        self.header_generation_number = ffff_hdr[7]
        FFFF_HDR_OFF_TAIL_SENTINEL = (self.header_size -
                                      FFFF_HDR_LEN_TAIL_SENTINEL)


        # Because we have variable-size FFFF headers, we need to recalculate
        # the number of entries in the section table, and the offsets to all
        # fields which follow.
        self.recalculate_header_offsets()

        # Unpack the 2nd sentinel at the tail
        ffff_hdr = unpack_from("<16s", self.ffff_buf,
                               FFFF_HDR_OFF_TAIL_SENTINEL)
        tail_sentinel = ffff_hdr[0]

        # Verify the sentinels
        if sentinel != FFFF_SENTINEL:
            raise ValueError("invalid sentinel")
        if sentinel != tail_sentinel != FFFF_SENTINEL:
            raise ValueError("invalid tail sentinel '{0:16s}'".format(tail_sentinel))

        # Validate the block size and image length
        if not is_power_of_2(self.erase_block_size):
            raise ValueError("Erase block size must be 2**n")
        elif (self.flash_image_length % self.erase_block_size) != 0:
            raise ValueError("Image length must be a multiple "
                             "of erase bock size")

        # Determine the ROM range that can hold the elements
        self.element_location_min = 2 * self.get_header_block_size()
        self.element_location_max = self.flash_capacity

    def add_element(self, element_type, element_class, element_id,
                    element_length, element_location, element_generation,
                    filename):
        # Add a new element to the element table but don't load the
        # TFTF file into the ROMimage buffer.  This is called for FFFF
        # creation, and adds the element to both FFFF headers.
        if self.ffff0 and self.ffff1:
            return \
                self.ffff0.add_element(element_type,
                                       element_class,
                                       element_id,
                                       element_length,
                                       element_location,
                                       element_generation,
                                       filename) and \
                self.ffff1.add_element(element_type,
                                       element_class,
                                       element_id,
                                       element_length,
                                       element_location,
                                       element_generation,
                                       filename)
        else:
            raise ValueError("No FFFF in which to add element")

    def post_process(self):
        """Post-process the FFFF header

        Reads the TFTF files into the ROMimage buffer for both FFFF headers.
        (Called by "create-ffff" after processing all arguments)
        """
        if self.ffff0 and self.ffff1:
            self.ffff0.post_process(self.mv)
            self.ffff1.post_process(self.mv)
        else:
            raise ValueError("No FFFF to post-process")

    def display(self, header_index, filename=None):
        """Display an FFFF header"""

        if self.ffff0 and self.ffff1:
            identical = self.ffff0.same_as(self.ffff1)
            self.ffff0.display(0, not identical, identical, filename)
            self.ffff1.display(1, True, identical, filename)
        else:
            raise ValueError("No FFFF to display")

    def write(self, out_filename):
        """Create the FFFF file

        Create the FFFF file, write the FFFF ROMimage buffer to it and return
        a success flag.  Appends the default FFFF file extension if omitted
        """
        # Reject the write if we didn't pass the sniff test
        if self.ffff0.header_validity != FFFF_HDR_VALID:
            raise ValueError("Invalid FFFF header 0")
        if self.ffff1.header_validity != FFFF_HDR_VALID:
            raise ValueError("Invalid FFFF header 1")

        # Ensure the output file ends in the default file extension if
        # the user hasn't specified their own extension.
        if rfind(out_filename, ".") == -1:
            out_filename += FFFF_FILE_EXTENSION

        # Output the entire FFFF blob
        with open(out_filename, 'wb') as wf:
            wf.write(self.ffff_buf)
            print("Wrote", out_filename)
            return True

    def explode(self, root_filename=None):
        """Write out the component elements

        Write the component elements out as separate files and return
        a success flag.  Appends the default FFFF file extension if omitted
        """

        if not root_filename:
            root_filename = "ffff"
        if self.ffff0.same_as(self.ffff1):
            self.ffff0.write_elements(root_filename)
        else:
            self.ffff0.write_elements(root_filename + "_0")
            self.ffff1.write_elements(root_filename + "_1")

    def create_map_file(self, base_name, base_offset):
        """Create a map file from the base name

        Create a map file from the base name substituting or adding ".map"
        as the file extension, and write out the offsets for the FFFF and
        TFTF fields.
        """
        index = base_name.rindex(".")
        if index != -1:
            base_name = base_name[:index]
        map_name = base_name + ".map"
        with open(map_name, 'w') as mapfile:
            self.write_map(mapfile, base_offset)

    def write_map(self, wf, base_offset):
        """Display the field names and offsets of an FFFF romimage"""
        if self.ffff0 and self.ffff1:
            self.ffff0.write_map(wf, 0, "ffff[0]")
            self.ffff1.write_map(wf, self.get_header_block_size(), "ffff[1]")
            if self.ffff0.same_as(self.ffff1):
                # The FFFF headers are identical, just traverse one for
                # the component TFTFs
                self.ffff0.write_map_elements(wf, 0, "ffff")
            else:
                # The FFFF headers are different, so traverse both for
                # the component TFTFs
                self.ffff0.write_map_elements(wf, 0, "ffff[0]")
                self.ffff1.write_map_elements(wf, 0, "ffff[1]")
        else:
            raise ValueError("No FFFF to display")
