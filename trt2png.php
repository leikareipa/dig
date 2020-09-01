<?php

/*
 * 2019 Tarpeeksi Hyvae Soft
 *
 * Software: trt2png
 * 
 * Converts dig's .TRT texture files into .PNG image files.
 * 
 * Usage:
 *  -i file    Path and filename of the input .TRT file.
 *  -o file    Path and filename of the output .PNG file.
 *  -p file    Path and filename of the palette .PAL file.
 * 
 */

$commandLine = getopt("i:o:p:");
{
    if (!isset($commandLine["i"]) ||
        !file_exists($commandLine["i"]) ||
        !file_exists($commandLine["i"] . ".mta"))
    {
        echo "No valid input file given.\n";
        exit(1);
    }

    if (!isset($commandLine["p"]) ||
        !file_exists($commandLine["p"]))
    {
        echo "No valid palette file given.\n";
        exit(1);
    }

    if (!isset($commandLine["o"]))
    {
        echo "No valid output file given.\n";
        exit(1);
    }
}

$metaData = explode(" ", file_get_contents("{$commandLine["i"]}.mta"));
$pixelData = array_values(unpack("C*", file_get_contents("{$commandLine["i"]}")));
$palette = array_values(unpack("C*", file_get_contents("{$commandLine["p"]}")));
$width = $metaData[0];
$height = $metaData[1];
$image = imagecreatetruecolor($width, $height);

imagealphablending($image, false);
imagesavealpha($image, true);

for ($y = 0; $y < $height; $y++)
{
    for ($x = 0; $x < $width; $x++)
    {
        $paletteIdx = $pixelData[($x + $y * $width)];
        
        $alpha = ($paletteIdx == 0)? 127 : 0;
        $red = $alpha? 0 : $palette[$paletteIdx*3+0];
        $green = $alpha? 0 : $palette[$paletteIdx*3+1];
        $blue = $alpha? 0 : $palette[$paletteIdx*3+2];

        $color = imagecolorallocatealpha($image, $red,
                                                 $green,
                                                 $blue,
                                                 $alpha);

        imagesetpixel($image, $x, $y, $color);
    }
}

imagepng($image, "{$commandLine["o"]}");

?>
