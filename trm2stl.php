<?php

/*
 * 2019 Tarpeeksi Hyvae Soft
 * trm2stl
 * 
 * Converts raw Tomb Raider 1 room mesh vertex arrays as exported by 'dig' into
 * STL mesh files.
 * 
 */

$faceData = explode("\n", file_get_contents("bin/output/mesh/room/1.trm"));

$outFile = fopen("bin/output/test.stl", "w");
if (!$outFile)
{
    error_log("Failed to open the output file.");
    exit(1);
}

fputs($outFile, "solid room\n");

foreach ($faceData as $face)
{
    $values = explode(" ", $face);
    $numVerts = $values[0];
    $components = array_slice($values, 1);

    // Quads (we'll triangulate them).
    if ($numVerts == 4)
    {
        fputs($outFile, "facet normal 0 0 0\n");
        fputs($outFile, "\touter loop\n");
        fputs($outFile, "\t\tvertex {$components[0]} {$components[1]} {$components[2]}\n");
        fputs($outFile, "\t\tvertex {$components[3]} {$components[4]} {$components[5]}\n");
        fputs($outFile, "\t\tvertex {$components[6]} {$components[7]} {$components[8]}\n");
        fputs($outFile, "\tendloop\n");
        fputs($outFile, "endfacet\n");

        fputs($outFile, "facet normal 0 0 0\n");
        fputs($outFile, "\touter loop\n");
        fputs($outFile, "\t\tvertex {$components[0]} {$components[1]} {$components[2]}\n");
        fputs($outFile, "\t\tvertex {$components[6]} {$components[7]} {$components[8]}\n");
        fputs($outFile, "\t\tvertex {$components[9]} {$components[10]} {$components[11]}\n");
        fputs($outFile, "\tendloop\n");
        fputs($outFile, "endfacet\n");
    }
    // Triangles.
    else if ($numVerts == 3)
    {
        fputs($outFile, "facet normal 0 0 0\n");
        fputs($outFile, "\touter loop\n");

        for ($p = 0; $p < $numVerts; $p++)
        {
            fputs($outFile, "\t\tvertex {$components[$p*3+0]} {$components[$p*3+1]} {$components[$p*3+2]}\n");
        }

        fputs($outFile, "\tendloop\n");
        fputs($outFile, "endfacet\n");
    }
}

?>
