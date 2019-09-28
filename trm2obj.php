<?php

/*
 * 2019 Tarpeeksi Hyvae Soft
 * trm2obj
 * 
 * Converts raw Tomb Raider 1 .TRM meshes as exported by 'dig' into .OBJ mesh
 * files.
 * 
 * Usage:
 *  -i path        Path to the input .TRM file.
 *  -o path        Path to the output .OBJ file.
 *  -m path        Path to the output .MTL file.
 *  -p path        Path to the level's .PAL palette file.
 *  -t path        Path to a directory containing Tomb Raider's object textures as .PNG files.
 * 
 */

$commandLine = getopt("i:o:t:m:p:");

if (!isset($commandLine["i"]) ||
    !file_exists($commandLine["i"]))
{
    echo "Invalid input path.\n";
    exit(1);
}

if (!isset($commandLine["o"]))
{
    echo "Invalid .OBJ output path.\n";
    exit(1);
}

if (!isset($commandLine["m"]))
{
    echo "Invalid .MTL output path.\n";
    exit(1);
}

if (!isset($commandLine["t"]))
{
    echo "Invalid texture path.\n";
    exit(1);
}

if (!isset($commandLine["p"]) ||
    !file_exists($commandLine["p"]))
{
    echo "Invalid palette path.\n";
    exit(1);
}

if ($commandLine["t"][strlen($commandLine["t"])-1] != "/")
{
    $commandLine["t"] .= "/";
}

$faceData = explode("\n", file_get_contents($commandLine["i"]));
$palette = array_values(unpack("C*", file_get_contents("{$commandLine["p"]}")));
$objFile = fopen($commandLine["o"], "w");
$mtlFile = fopen($commandLine["m"], "w");

if (!$objFile ||
    !$mtlFile)
{
    error_log("Failed to open the output file.");
    exit(1);
}

fputs($objFile, "# A conversion produced by dig/trm2obj of a Tomb Raider 1 mesh.\n");
fputs($objFile, "mtllib " . basename($commandLine["m"]) . "\n");
fputs($objFile, "o tr_mesh\n");

/* Export the materials.*/
$knownMaterials = [];
foreach ($faceData as $face)
{
    if (empty($face))
    {
        continue;
    }

    $values = explode(" ", $face);
    if (!count($values))
    {
        continue;
    }

    $textureIdx = $values[1];

    if ($textureIdx >= 0)
    {
        if (isset($knownMaterials[$textureIdx]))
        {
            continue;
        }

        fputs($mtlFile, "newmtl object_texture_{$textureIdx}\n");
        fputs($mtlFile, "Kd 1 1 1\n");
        fputs($mtlFile, "Ks 0 0 0\n");
        fputs($mtlFile, "Ns 0\n");
        fputs($mtlFile, "illum 0\n");
        fputs($mtlFile, "map_Kd {$commandLine["t"]}{$textureIdx}.png\n");

        $knownMaterials[$textureIdx] = true;
    }
    else
    {
        $textureIdx = abs($textureIdx);
        $paletteIdx = ($textureIdx * 3);

        if (isset($knownMaterials["color_" . $textureIdx]))
        {
            continue;
        }

        fputs($mtlFile, "newmtl object_color_{$textureIdx}\n");
        fprintf($mtlFile, "Kd %f %f %f\n", ($palette[$paletteIdx+0] / 255.0),
                                           ($palette[$paletteIdx+1] / 255.0),
                                           ($palette[$paletteIdx+2] / 255.0));
        fputs($mtlFile, "Ks 0 0 0\n");
        fputs($mtlFile, "Ns 0\n");
        fputs($mtlFile, "illum 0\n");

        $knownMaterials["color_" . $textureIdx] = true;
    }
    

    /* Separate the next material block from the current one.*/
    fputs($mtlFile, "\n");
}

/* Export the vertices.*/
foreach ($faceData as $face)
{
    if (empty($face))
    {
        continue;
    }

    $values = explode(" ", $face);
    if (!count($values))
    {
        continue;
    }

    $numVerts = $values[0];
    if (!$numVerts)
    {
        continue;
    }
    
    $textureIdx = $values[1];
    $components = array_slice($values, 2);

    for ($p = 0; $p < $numVerts; $p++)
    {
        // Each vertex has 5 values: x, y, z, u, v.
        fputs($objFile, "v {$components[$p*5+0]} {$components[$p*5+1]} {$components[$p*5+2]}\n");
    }
}

/* Export the uv coordinates.*/
foreach ($faceData as $face)
{
    if (empty($face))
    {
        continue;
    }

    $values = explode(" ", $face);
    if (!count($values))
    {
        continue;
    }

    $numVerts = $values[0];
    if (!$numVerts)
    {
        continue;
    }
    
    $textureIdx = $values[1];
    $components = array_slice($values, 2);

    for ($p = 0; $p < $numVerts; $p++)
    {
        // Each vertex has 5 values: x, y, z, u, v.
        fputs($objFile, "vt {$components[$p*5+3]} {$components[$p*5+4]}\n");
    }
}

/* Export the faces.*/
$idx = 1;
foreach ($faceData as $face)
{
    if (empty($face))
    {
        continue;
    }

    $values = explode(" ", $face);
    if (!count($values))
    {
        continue;
    }

    $numVerts = $values[0];
    if (!$numVerts)
    {
        continue;
    }

    $textureIdx = $values[1];
    $components = array_slice($values, 2);

    if ($textureIdx >= 0)
    {
        fputs($objFile, "usemtl object_texture_{$textureIdx}\n");
    }
    else
    {
        fprintf($objFile, "usemtl object_color_%d\n", abs($textureIdx));
    }
    fputs($objFile, "f");
    for ($p = 0; $p < $numVerts; $p++)
    {
        fputs($objFile, " {$idx}/{$idx}");
        $idx++;
    }
    fputs($objFile, "\n");
}

?>
