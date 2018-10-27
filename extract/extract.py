import argparse
import logging
import os
import shutil
import struct
import itertools

from PIL import Image


logger = logging.getLogger('extract')
logging.basicConfig(level='DEBUG')

# Image format: (little endian to match arduino cpu)
# 4b magic, 2b version, 2b header offset
# v1:
# header:
# 2b pixel w, 2b pixel h, 1b bpc (bits per channel - 24bit is 8bpc, 3 channels (rgb) assumed)
# [blank - until offset]
# Image sets are prefixed with 1b loop count - 0 do not loop, >0 loop that many times
# Image data: stored in rows, top to bottom, ltr
# 1b command, 1b argument, followed by data if applicable
# Commands:
# 1: end of image in image set
# 2: end of image set
# 3: write pixel data (data is stored as r,g,b bytes or shorts depending on bpc, for 24 bit, 3 bytes per pixel, repeating)
# 4: write pixel data, rle (data is a single r,g,b byte tuple or short)
# For writing pixels, argument is number of pixels to write - 1 (0 means 1 pixel, 255 means 256 pixels, doesn't make sense to write 0 pixels)
# 5: seek (arg is 0, data is 2b x, 2b y)
# 6: delay ms (argument is number of ms to delay, INCLUDING time to write data, no extra data is needed for this)
# reset back to first image set after the last one


class AnimImg(object):
    MAGIC_FMT = '<IH'
    VERSION = None

    CMD_START_SET = 1
    CMD_END_IMG = 2
    CMD_END_SET = 3
    CMD_PIX_RAW = 4
    CMD_PIX_RLE = 5
    CMD_SEEK = 6
    CMD_DELAY = 7

    def __init__(self, dimensions, filenames, output):
        self.dimensions = dimensions
        self.filenames = filenames
        self.output = output

    def crop_resize_img(self, img):
        # https://gist.github.com/sigilioso/2957026#gistcomment-970757
        img_ratio = img.size[0] / float(img.size[1])
        ratio = self.dimensions[0] / float(self.dimensions[1])
        #The image is scaled/cropped vertically or horizontally depending on the ratio
        if ratio > img_ratio:
            img = img.resize((self.dimensions[0], round(self.dimensions[0] * img.size[1] / img.size[0])),
                    Image.ANTIALIAS)
            box = (0, round((img.size[1] - self.dimensions[1]) / 2), img.size[0],
                   round((img.size[1] + self.dimensions[1]) / 2))
            img = img.crop(box)
        elif ratio < img_ratio:
            img = img.resize((round(self.dimensions[1] * img.size[0] / img.size[1]), self.dimensions[1]),
                    Image.ANTIALIAS)
            box = (round((img.size[0] - self.dimensions[0]) / 2), 0,
                   round((img.size[0] + self.dimensions[0]) / 2), img.size[1])
            img = img.crop(box)
        else:
            img = img.resize((self.dimensions[0], self.dimensions[1]),
                    Image.ANTIALIAS)
            # If the scale is the same, we do not need to crop

        return img

    def get_frames(self, filename):
        logger.debug("Open %s", filename)
        orig_img = Image.open(filename)
        if orig_img.is_animated:
            logger.debug("%s is animated, returning frames", filename)
            for frame_num in range(orig_img.n_frames):
                orig_img.seek(frame_num)
                frame = orig_img
                if frame.mode != 'RGB':
                    logger.debug("Converting %s:%d to RGB", filename, frame_num)
                    frame = frame.convert('RGB')
                yield ((frame_num + 1) == orig_img.n_frames), frame.info['duration'], frame
        else:
            logger.debug("%s is not animated, returning single image", filename)
            if img.mode != 'RGB':
                logger.debug("Converting %s to RGB", filename)
                img = img.convert('RGB')
            yield True, None, img

    def get_pixels(self, img):
        w, h = img.size
        for y in range(h):
            for x in range(w):
                yield img.getpixel((x, y))

    def get_pixels_rle(self, pixels, max_chunk=255):
        def chunk_list(data):
            out = []
            for v in data:
                out.append(v)
                if len(out) == max_chunk:
                    yield out
                    out = []
            if out:
                yield out

        all_pixels = list(pixels)
        assert len(all_pixels) == (self.dimensions[0] * self.dimensions[1])

        total_px = 0
        out = list(map(lambda v: list(v[1]), itertools.groupby(all_pixels)))
        # out is a list of lists of pixel values that are all the same
        raw_px = []
        for group in out:
            if len(group) > 3:
                # The group is a long enough run of pixels to RLE-encode
                if raw_px:
                    for v in chunk_list(raw_px):
                        total_px += len(v)
                        yield 0, v
                    raw_px = []
                for v in chunk_list(group):
                    total_px += len(v)
                    yield len(v), [v[0]]
            else:
                raw_px += group
        if raw_px:
            for v in chunk_list(raw_px):
                total_px += len(v)
                yield 0, v

        assert total_px == (self.dimensions[0] * self.dimensions[1])

    def get_magic(self):
        raise NotImplementedError()

    def get_header(self):
        raise NotImplementedError()

    def get_commands(self):
        raise NotImplementedError()

    def write(self):
        with open(self.output, 'wb') as fp:
            fp.write(self.get_magic())
            fp.write(self.get_header())
            for chunk in self.get_commands():
                fp.write(chunk)


class AnimImgV2(AnimImg):
    VERSION = 2
    HEADER_FMT = '<HH'
    LOOP_COUNT_FMT = '<B'
    COMMAND_FMT = '<BB'
    PIXEL_FMT = '<H'
    SEEK_FMT = '<HH'

    def _color_565(self, pixel):
        red, green, blue = pixel
        return ((red & 0xF8) << 8) | ((green & 0xFC) << 3) | ((blue & 0xF8) >> 3);

    def get_magic(self):
        return struct.pack(self.MAGIC_FMT, 0x676d4941, self.VERSION)

    def get_header(self):
        return struct.pack(self.HEADER_FMT, self.dimensions[0], self.dimensions[1])

    def get_commands(self):
        for filename in self.filenames:
            yield struct.pack(self.COMMAND_FMT, self.CMD_START_SET, 0)
            for _, duration, frame in self.get_frames(filename):
                frame = self.crop_resize_img(frame)
                pixel_groups = list(self.get_pixels_rle(self.get_pixels(frame)))
                for num, data in pixel_groups:
                    if num == 0:
                        # Write raw pixel data
                        yield struct.pack(
                            self.COMMAND_FMT,
                            self.CMD_PIX_RAW,
                            len(data)
                        ) + b''.join([
                            struct.pack(self.PIXEL_FMT, self._color_565(d))
                            for d in data
                        ])
                    else:
                        # Write rle encoded pixel data
                        yield struct.pack(self.COMMAND_FMT, self.CMD_PIX_RLE, num) + struct.pack(self.PIXEL_FMT, self._color_565(data[0]))
                # End of img in set
                yield struct.pack(self.COMMAND_FMT, self.CMD_END_IMG, 0)
                # Delay
                while duration > 255:
                    yield struct.pack(self.COMMAND_FMT, self.CMD_DELAY, 255)
                    duration -= 255
                yield struct.pack(self.COMMAND_FMT, self.CMD_DELAY, duration)
            # End of set
            yield struct.pack(self.COMMAND_FMT, self.CMD_END_SET, 0)


def parse_args():
    parser = argparse.ArgumentParser(description="Pack multiple images or animated gifs into a format suitable for arduino")
    parser.add_argument('-o', '--output', default='main.anim', help="Output file, will be created if it does not exist, will be overwritten if it does")
    parser.add_argument('filenames', nargs='*', help="Image/GIF filenames to extract")
    return parser.parse_args()


if __name__ == '__main__':
    args = parse_args()
    out = AnimImgV2((128, 128), args.filenames, args.output)
    out.write()
