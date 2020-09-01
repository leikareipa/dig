<?php

/*
 * 2019 Tarpeeksi Hyvae Soft
 * 
 * Software: trt2png
 * 
 * Converts dig's .TRM texture files into .OBJ mesh files.
 * 
 * Usage:
 *  -i file         Path and filename of the input .TRM file.
 *  -o file         Path and filename of the output .OBJ file.
 *  -m file         Path and filename of the output .MTL file.
 *  -p file         Path and filename of the level's .PAL palette file.
 *  -t directory    Path to a directory containing Tomb Raider's object textures as .PNG files.
 * 
 */

$commandLine = getopt("i:o:t:m:p:");

// Verify command-line arguments.
{
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
        $commandLine["t"] = "";
    }
    else if ($commandLine["t"][strlen($commandLine["t"])-1] != "/")
    {
        $commandLine["t"] .= "/";
    }

    if (!isset($commandLine["p"]) ||
        !file_exists($commandLine["p"]))
    {
        echo "Invalid palette path.\n";
        exit(1);
    }
}

// Obtain input data.
{
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
}

// Export.
{
    fputs($objFile, "# A conversion produced by dig/trm2obj of a Tomb Raider 1 mesh.\n");
    fputs($objFile, "mtllib " . basename($commandLine["m"]) . "\n");
    fputs($objFile, "o tr_mesh\n");

    export_materials($mtlFile, $faceData, $palette, $commandLine["t"]);
    $uniqueVertexList = export_vertex_list($objFile, $faceData);
    $uniqueUVList = export_uv_list($objFile, $faceData);
    export_faces($objFile, $faceData, $uniqueVertexList, $uniqueUVList);
}

exit(0);

function export_materials($outputFile, array $faces, array $palette, string $texturePath)
{
    // A list of the materials we've already exported. Used to make sure we don't
    // export the same material twice.
    $knownMaterials = [];

    foreach ($faces as $face)
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

            fputs($outputFile, "newmtl object_texture_{$textureIdx}\n");
            fputs($outputFile, "Kd 1 1 1\n");
            fputs($outputFile, "Ks 0 0 0\n");
            fputs($outputFile, "Ns 0\n");
            fputs($outputFile, "illum 0\n");
            fputs($outputFile, "map_Kd {$texturePath}{$textureIdx}.png\n");

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

            fputs($outputFile, "newmtl object_color_{$textureIdx}\n");
            fprintf($outputFile, "Kd %f %f %f\n", ($palette[$paletteIdx+0] / 255.0),
                                                  ($palette[$paletteIdx+1] / 255.0),
                                                  ($palette[$paletteIdx+2] / 255.0));
            fputs($outputFile, "Ks 0 0 0\n");
            fputs($outputFile, "Ns 0\n");
            fputs($outputFile, "illum 0\n");

            $knownMaterials["color_" . $textureIdx] = true;
        }
        
        /* Separate the next material block from the current one.*/
        fputs($outputFile, "\n");
    }
}

// Saves a list of the given faces' unique vertices into the output file, and returns
// the list.
function export_vertex_list($outputFile, $faces) : UniqueArrayList
{
    // Create a list of unique vertices.
    $vertexList = new UniqueArrayList();
    foreach ($faces as $face)
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
        
        $components = array_slice($values, 2);

        for ($p = 0; $p < $numVerts; $p++)
        {
            // Each vertex has 5 values: x, y, z, u, v.
            $x = $components[$p*5+0];
            $y = $components[$p*5+1];
            $z = $components[$p*5+2];

            $vertexList->add(["x"=>$x, "y"=>$y, "z"=>$z]);
        }
    }

    // Export the unique list of vertices.
    for ($i = 0; $i < $vertexList->count(); $i++)
    {
        $vertex = $vertexList->array_at($i);
        fputs($outputFile, "v {$vertex["x"]} {$vertex["y"]} {$vertex["z"]}\n");
    }

    return $vertexList;
}

function export_uv_list($outputFile, array $faces) : UniqueArrayList
{
    // Create a list of unique UV coordinates.
    $uvList = new UniqueArrayList();
    foreach ($faces as $face)
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
            $u = $components[$p*5+3];
            $v = $components[$p*5+4];

            $uvList->add(["u"=>$u, "v"=>$v]);
        }
    }

    /* Export the list of UV coordinates.*/
    for ($i = 0; $i < $uvList->count(); $i++)
    {
        $uv = $uvList->array_at($i);
        fputs($outputFile, "vt {$uv["u"]} {$uv["v"]}\n");
    }

    return $uvList;
}

function export_faces($outputFile, array $faces, UniqueArrayList $uniqueVertexList, UniqueArrayList $uniqueUVList)
{
    /* Export the faces.*/
    foreach ($faces as $face)
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
            fputs($outputFile, "usemtl object_texture_{$textureIdx}\n");
        }
        else
        {
            fprintf($outputFile, "usemtl object_color_%d\n", abs($textureIdx));
        }
        fputs($outputFile, "f");
        for ($p = 0; $p < $numVerts; $p++)
        {
            $x = $components[$p*5+0];
            $y = $components[$p*5+1];
            $z = $components[$p*5+2];
            $u = $components[$p*5+3];
            $v = $components[$p*5+4];

            $vertexListIdx = ($uniqueVertexList->array_key(["x"=>$x, "y"=>$y, "z"=>$z]) + 1);
            $uvListIdx = ($uniqueUVList->array_key(["u"=>$u, "v"=>$v]) + 1);

            fputs($outputFile, " {$vertexListIdx}/{$uvListIdx}");
        }
        fputs($outputFile, "\n");
    }

    return;
}

// Maintains a list of arrays such that:
//
//   - Each array has the same number of components
//   - No two arrays have the same component values
//
class UniqueArrayList
{
    private $list;

    function __construct()
    {
        $this->list = [];
    }

    function add(array $componentArray)
    {
        if (!$this->item_exists_in_list($componentArray))
        {
            $this->list[] = $componentArray;
        }
    }

    // Returns the component values of the item in the given index position in the list;
    // or false if the index is out of bounds.
    function array_at(int $idx)
    {
        if (($idx < 0) ||
            ($idx >= $this->count()))
        {
            return false;
        }

        return $this->list[$idx];
    }

    function count() : int
    {
        return count($this->list);
    }

    // Returns the key in the list of the first element whose component values match the
    // given ones; or false if no such match is found.
    function array_key(array $componentArray)
    {
        foreach ($this->list as $listItemKey=>$listItem)
        {
            if (count($componentArray) !== count($listItem))
            {
                error_log("UniqueArrayList: Mismatching component count.");
                return false;
            }

            $foundAll = true;
            foreach ($componentArray as $componentKey=>$component)
            {
                if (!isset($listItem[$componentKey]))
                {
                    error_log("UniqueArrayList: Incompatible components.");
                    return false;
                }

                if ($listItem[$componentKey] != $component)
                {
                    $foundAll = false;
                    break;
                }
            }

            if ($foundAll)
            {
                return $listItemKey;
            }
        }

        return false;
    }

    // Returns true if a list item by the given components exists in the list.
    function item_exists_in_list(array $componentArray) : bool
    {
        return ($this->array_key($componentArray) !== false);
    }
}

?>
