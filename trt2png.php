<?php

/*
 * 2019 Tarpeeksi Hyvae Soft
 * trt2png
 * 
 * Converts raw Tomb Raider 1 .TRT textures as exported by 'dig' into .PNG files.
 * 
 * Usage:
 *  -i path        Path to the input .TRT file.
 *  -o path        Path to the output .PNG file.
 *  -p path        Path to the palette .PAL file.
 * 
 */

$commandLine = getopt("i:o:p:");

if (!isset($commandLine["i"]) ||
    !file_exists($commandLine["i"]) ||
    !file_exists($commandLine["i"] . ".mta"))
{
    echo "Invalid input path.\n";
    exit(1);
}

if (!isset($commandLine["p"]) ||
    !file_exists($commandLine["p"]))
{
    echo "Invalid palette path.\n";
    exit(1);
}

if (!isset($commandLine["o"]))
{
    echo "Invalid output path.\n";
    exit(1);
}

$metaData = explode(" ", file_get_contents("{$commandLine["i"]}.mta"));
$pixelData = array_values(unpack("C*", file_get_contents("{$commandLine["i"]}")));
$palette = array_values(unpack("C*", file_get_contents("{$commandLine["p"]}")));
$width = $metaData[0];
$height = $metaData[1];
$image = imagecreatetruecolor($width, $height);

for ($y = 0; $y < $height; $y++)
{
    for ($x = 0; $x < $width; $x++)
    {
        $paletteIdx = $pixelData[($x + $y * $width)];

        $color = imageColorAllocate($image, $palette[$paletteIdx*3+0],
                                            $palette[$paletteIdx*3+1],
                                            $palette[$paletteIdx*3+2]);

        imagesetpixel($image, $x, $y, $color);
    }
}

imagepng($image, "{$commandLine["o"]}");

?>
