/*
 * 2019 Tarpeeksi Hyvae Soft
 * 
 * Software: dig
 * 
 * 
 * Exports mesh and texture data from a given Tomb Raider 1 PHD file.
 * 
 * You should first create the following directory structure under where you
 * run the program:
 * 
 *   dig's directory
 *   |
 *   +- output
 *      |
 *      +- mesh
 *      |  |
 *      |  +- room
 *      |
 *      +- texture
 *         |
 *         +- atlas
 *         |
 *         +- object
 * 
 * Based on the third-party file format documentation available at
 * https://trwiki.earvillage.net/doku.php?id=trs:file_formats.
 *
 */

#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>

/* Placeholder byte sizes of various Tomb Raider data structs.*/
#define SIZE_TR_ROOM_STATIC_MESH 18
#define SIZE_TR_CINEMATIC_FRAME 16
#define SIZE_TR_SPRITE_SEQUENCE 8
#define SIZE_TR_OBJECT_TEXTURE 20
#define SIZE_TR_SPRITE_TEXTURE 16
#define SIZE_TR_MESH_TREE_NODE 4
#define SIZE_TR_ANIM_DISPATCH 8
#define SIZE_TR_ANIM_COMMANDS 2
#define SIZE_TR_SOUND_SOURCE 16
#define SIZE_TR_STATE_CHANGE 6
#define SIZE_TR_STATIC_MESH 32
#define SIZE_TR_ROOM_PORTAL 32
#define SIZE_TR_ROOM_SECTOR 8
#define SIZE_TR_ROOM_LIGHT 18
#define SIZE_TR_ROOM_INFO 16
#define SIZE_TR_ANIMATION 32
#define SIZE_TR_ENTITY 22
#define SIZE_TR_CAMERA 16
#define SIZE_TR_VERTEX 6
#define SIZE_TR_MODEL 18
#define SIZE_TR_FACE4 12
#define SIZE_TR_FACE3 8
#define SIZE_TR_BOX 20

struct tr_object_texture_s
{
    unsigned width, height;
    unsigned hasAlpha;
    unsigned hasWireframe;
    unsigned ignoresDepthTest;

    /* UV coordinates for each of the texture's four corners.*/
    float u[4];
    float v[4];

    uint8_t *pixelData;
};

struct tr_vertex_s
{
    int x, y, z;
    int lighting;
};

struct tr_quad_s
{
    struct tr_vertex_s vertex[4];
    unsigned isDoubleSided;
    int textureIdx;
};

struct tr_triangle_s
{
    struct tr_vertex_s vertex[3];
    unsigned isDoubleSided;
    int textureIdx;
};

/* The 3d mesh.*/
struct tr_mesh_s
{
    unsigned numTexturedQuads;
    struct tr_quad_s *texturedQuads;

    unsigned numTexturedTriangles;
    struct tr_triangle_s *texturedTriangles;

    unsigned numUntexturedQuads;
    struct tr_quad_s *untexturedQuads;

    unsigned numUntexturedTriangles;
    struct tr_triangle_s *untexturedTriangles;
};

/* Metadata about a 3d mesh.*/
struct tr_mesh_meta_s
{
    /* The object's world coordinates.*/
    int x, y, z;

    /* Each object can optionally be rotated one or more times by 90 degrees
     * horizontally.*/
    unsigned rotation;

    /* How light or dark the object is (between 0 = light and 8191 = dark).*/
    unsigned lighting;

    /* An index to the master list of room object meshes specifying this object's
     * mesh.*/
    unsigned meshIdx;
};

/* The 3d mesh of a room.*/
struct tr_room_mesh_s
{
    /* The room's world coordinates.*/
    int x, y, z;

    unsigned numQuads;
    struct tr_quad_s *quads;

    unsigned numTriangles;
    struct tr_triangle_s *triangles;

    /* In addition to its own geometry, a room may optionally include a number
     * of static objects.*/
    unsigned numStaticObjects;
    struct tr_mesh_meta_s *staticObjects;
};

struct tr_texture_atlas_s
{
    unsigned width, height;
    uint8_t *pixelData;
};

/* The data we've loaded from the level file (but not necessarily in the same
 * format).*/
struct imported_data_s
{
    unsigned fileVersion;

    uint8_t *palette;

    unsigned numTextureAtlases;
    struct tr_texture_atlas_s *textureAtlases;

    unsigned numRoomMeshes;
    struct tr_room_mesh_s *roomMeshes;

    unsigned numObjectTextures;
    struct tr_object_texture_s *objectTextures;

    /* The master list of meshes.*/
    unsigned numMeshes;
    struct tr_mesh_s *meshes;
};

static FILE *INPUT_FILE;
static struct imported_data_s IMPORTED_DATA;

int32_t read_value(const unsigned numBytes)
{
    int32_t value = 0;
    size_t read = 0;

    assert((numBytes > 0) && "Can't read a 0-byte value.");
    assert((numBytes <= sizeof(value)) && "Asked to read a value larger than the input buffer.");
    
    read = fread((char*)&value, 1, numBytes, INPUT_FILE);

    assert((read == numBytes) && "Failed to correctly read from the input file.");

    return value;
}

void read_bytes(char *const dst, const unsigned numBytes)
{
    const size_t read = fread(dst, 1, numBytes, INPUT_FILE);

    assert((read == numBytes) && "Failed to correctly read from the input file.");

    return;
}

void skip_num_bytes(const unsigned numBytes)
{
    const int seek = fseek(INPUT_FILE, numBytes, SEEK_CUR);

    assert((seek == 0) && "Failed to correctly seek in the input file.");

    return;
}

void print_file_pos(const int offset)
{
    printf("%ld", (ftell(INPUT_FILE) + offset));

    return;
}

void import_data_from_input_file(void)
{
    int i = 0, p = 0;

    IMPORTED_DATA.fileVersion = (uint32_t)read_value(4);

    assert((IMPORTED_DATA.fileVersion == 32) && "Expected a Tomb Raider 1 level file.");

    /* Read textures.*/
    {
        IMPORTED_DATA.numTextureAtlases = read_value(4);
        IMPORTED_DATA.textureAtlases = malloc(sizeof(struct tr_texture_atlas_s) * IMPORTED_DATA.numTextureAtlases);

        for (i = 0; i < IMPORTED_DATA.numTextureAtlases; i++)
        {
            unsigned numPixels;

            IMPORTED_DATA.textureAtlases[i].width = 256;
            IMPORTED_DATA.textureAtlases[i].height = 256;
            numPixels = (IMPORTED_DATA.textureAtlases[i].width * IMPORTED_DATA.textureAtlases[i].height);

            IMPORTED_DATA.textureAtlases[i].pixelData = malloc(numPixels);
            read_bytes((char*)IMPORTED_DATA.textureAtlases[i].pixelData, numPixels);
        }
    }

    /* Skip unknown dword.*/
    skip_num_bytes(4);

    /* Read rooms.*/
    {
        IMPORTED_DATA.numRoomMeshes = read_value(2);
        print_file_pos(-2);printf(" Rooms: %d\n", IMPORTED_DATA.numRoomMeshes);

        IMPORTED_DATA.roomMeshes = malloc(sizeof(struct tr_room_mesh_s) * IMPORTED_DATA.numRoomMeshes);

        for (i = 0; i < IMPORTED_DATA.numRoomMeshes; i++)
        {
            unsigned numRoomDataWords = 0;
            unsigned numPortals = 0;
            unsigned numZSectors = 0;
            unsigned numXSectors = 0;
            int ambientIntensity = 0;
            unsigned numLights = 0;
            unsigned alternateRoom = 0;
            int16_t flags = 0;

            print_file_pos(0);printf("   #%d\n", i);

            /* Room info.*/
            IMPORTED_DATA.roomMeshes[i].x = read_value(4);
            IMPORTED_DATA.roomMeshes[i].z = read_value(4);
            skip_num_bytes(4); /* Skip 'yBottom'.*/
            skip_num_bytes(4); /* Skip 'yTop'.   */

            /* Room data.*/
            {
                char *rawRoomMeshData = NULL;

                numRoomDataWords = read_value(4);
                print_file_pos(-2);printf("     Room data size: %d\n", numRoomDataWords);

                rawRoomMeshData = malloc(numRoomDataWords * 2);
                read_bytes(rawRoomMeshData, (numRoomDataWords * 2));

                /* Parse the raw room mesh data.*/
                {
                    struct tr_vertex_s *vertexList = NULL;
                    const int16_t *roomDataIterator = (int16_t*)&(rawRoomMeshData[0]);

                    /* Vertex list.*/
                    {
                        const unsigned numVertices = *roomDataIterator++;
                        print_file_pos(0);printf("       Vertices: %d\n", numVertices);

                        vertexList = malloc(sizeof(struct tr_vertex_s) * numVertices);

                        for (p = 0; p < numVertices; p++)
                        {
                            vertexList[p].x = (*roomDataIterator++ + IMPORTED_DATA.roomMeshes[i].x);
                            vertexList[p].y = *roomDataIterator++;
                            vertexList[p].z = (*roomDataIterator++ + IMPORTED_DATA.roomMeshes[i].z);
                            vertexList[p].lighting = *roomDataIterator++;
                        }
                    }

                    /* Quads.*/
                    {
                        IMPORTED_DATA.roomMeshes[i].numQuads = *roomDataIterator++;
                        print_file_pos(0);printf("       Quads: %d\n", IMPORTED_DATA.roomMeshes[i].numQuads);

                        IMPORTED_DATA.roomMeshes[i].quads = malloc(sizeof(struct tr_quad_s) * IMPORTED_DATA.roomMeshes[i].numQuads);

                        for (p = 0; p < IMPORTED_DATA.roomMeshes[i].numQuads; p++)
                        {
                            IMPORTED_DATA.roomMeshes[i].quads[p].vertex[0] = vertexList[*roomDataIterator++];
                            IMPORTED_DATA.roomMeshes[i].quads[p].vertex[1] = vertexList[*roomDataIterator++];
                            IMPORTED_DATA.roomMeshes[i].quads[p].vertex[2] = vertexList[*roomDataIterator++];
                            IMPORTED_DATA.roomMeshes[i].quads[p].vertex[3] = vertexList[*roomDataIterator++];
                            IMPORTED_DATA.roomMeshes[i].quads[p].textureIdx = *roomDataIterator++;

                            IMPORTED_DATA.roomMeshes[i].quads[p].isDoubleSided = (IMPORTED_DATA.roomMeshes[i].quads[p].textureIdx & 0x8000);
                            IMPORTED_DATA.roomMeshes[i].quads[p].textureIdx = (IMPORTED_DATA.roomMeshes[i].quads[p].textureIdx & 0x7fff);
                        }
                    }

                    /* Triangles.*/
                    {
                        IMPORTED_DATA.roomMeshes[i].numTriangles = *roomDataIterator++;
                        print_file_pos(0);printf("       Triangles: %d\n", IMPORTED_DATA.roomMeshes[i].numTriangles);

                        IMPORTED_DATA.roomMeshes[i].triangles = malloc(sizeof(struct tr_triangle_s) * IMPORTED_DATA.roomMeshes[i].numTriangles);

                        for (p = 0; p < IMPORTED_DATA.roomMeshes[i].numTriangles; p++)
                        {
                            IMPORTED_DATA.roomMeshes[i].triangles[p].vertex[0] = vertexList[*roomDataIterator++];
                            IMPORTED_DATA.roomMeshes[i].triangles[p].vertex[1] = vertexList[*roomDataIterator++];
                            IMPORTED_DATA.roomMeshes[i].triangles[p].vertex[2] = vertexList[*roomDataIterator++];
                            IMPORTED_DATA.roomMeshes[i].triangles[p].textureIdx = *roomDataIterator++;

                            IMPORTED_DATA.roomMeshes[i].triangles[p].isDoubleSided = (IMPORTED_DATA.roomMeshes[i].triangles[p].textureIdx & 0x8000);
                            IMPORTED_DATA.roomMeshes[i].triangles[p].textureIdx = (IMPORTED_DATA.roomMeshes[i].triangles[p].textureIdx & 0x7fff);
                        }
                    }

                    free(vertexList);
                }

                free(rawRoomMeshData);
            }

            /* Portals.*/
            numPortals = read_value(2);
            print_file_pos(-2);printf("     Portals: %d\n", numPortals);
            skip_num_bytes(SIZE_TR_ROOM_PORTAL * numPortals);

            /* Sectors.*/
            numZSectors = read_value(2);
            numXSectors = read_value(2);
            print_file_pos(-4);printf("     Sectors: %d, %d\n", numZSectors, numXSectors);
            skip_num_bytes(SIZE_TR_ROOM_SECTOR * numZSectors * numXSectors);

            /* Lights.*/
            ambientIntensity = (int16_t)read_value(2);
            numLights = read_value(2);
            print_file_pos(-4);printf("     Lights: %d (%d)\n", numLights, ambientIntensity);
            skip_num_bytes(SIZE_TR_ROOM_LIGHT * numLights);

            /* Static room meshes.*/
            IMPORTED_DATA.roomMeshes[i].numStaticObjects = read_value(2);
            print_file_pos(-2);printf("     Static meshes: %d\n", IMPORTED_DATA.roomMeshes[i].numStaticObjects);
            IMPORTED_DATA.roomMeshes[i].staticObjects = malloc(sizeof(struct tr_mesh_meta_s) * IMPORTED_DATA.roomMeshes[i].numStaticObjects);
            for (p = 0; p < IMPORTED_DATA.roomMeshes[i].numStaticObjects; p++)
            {
                IMPORTED_DATA.roomMeshes[i].staticObjects[p].x = read_value(4);
                IMPORTED_DATA.roomMeshes[i].staticObjects[p].y = read_value(4);
                IMPORTED_DATA.roomMeshes[i].staticObjects[p].z = read_value(4);
                IMPORTED_DATA.roomMeshes[i].staticObjects[p].rotation = ((read_value(2) & 0xc000) >> 14);
                IMPORTED_DATA.roomMeshes[i].staticObjects[p].lighting = read_value(2);
                IMPORTED_DATA.roomMeshes[i].staticObjects[p].meshIdx = read_value(2);
            }

            /* Miscellaneous.*/
            alternateRoom = read_value(2);
            flags = read_value(2);
            print_file_pos(-2);printf("     Alternate room: %d\n", alternateRoom);
            print_file_pos(-2);printf("     Flags: 0x%x\n", flags);
        }
    }

    /* Read floors.*/
    {
        const unsigned numFloors = read_value(4);
        skip_num_bytes(numFloors * 2);
    }

    /* Read meshes.*/
    {
        /* The raw mesh data array.*/
        char *meshData = NULL;
        unsigned meshDataLength = 0;
        int16_t *meshDataIterator = NULL; /* For iterating through and extracting values from the array.*/

        /* Offsets to the raw mesh data array of objects' mesh data.*/
        unsigned *meshOffsets = NULL;

        meshDataLength = (read_value(4) * 2);
        meshData = malloc(meshDataLength);
        read_bytes(meshData, meshDataLength);

        IMPORTED_DATA.numMeshes = read_value(4);
        print_file_pos(0);printf(" Meshes: %d\n", IMPORTED_DATA.numMeshes);
        IMPORTED_DATA.meshes = malloc(sizeof(struct tr_mesh_s) * IMPORTED_DATA.numMeshes);
        meshOffsets = malloc(sizeof(meshOffsets) * IMPORTED_DATA.numMeshes);
        read_bytes((char*)meshOffsets, (sizeof(uint32_t) * IMPORTED_DATA.numMeshes));

        /* Extract individual meshes from the raw mesh data array.*/
        for (i = 0; i < IMPORTED_DATA.numMeshes; i++)
        {
            struct tr_mesh_s *const objectMesh = &IMPORTED_DATA.meshes[i];
            struct tr_vertex_s *vertexList = NULL;
            int numVertices = 0;
            int numNormals = 0;

            meshDataIterator = (int16_t*)(meshData + meshOffsets[i]);

            /* Skip vertex 'center'.*/
            meshDataIterator += (SIZE_TR_VERTEX / 2);

            /* Skip uint32_t collisionRadius.*/
            meshDataIterator += (sizeof(uint32_t) / 2);
            
            numVertices = *meshDataIterator++;
            vertexList = malloc(sizeof(struct tr_vertex_s) * numVertices);
            for (p = 0; p < numVertices; p++)
            {
                vertexList[p].x = *meshDataIterator++;
                vertexList[p].y = *meshDataIterator++;
                vertexList[p].z = *meshDataIterator++;
                vertexList[p].lighting = 0; /* Object meshes have no pre-baked lighting.*/
            }

            numNormals = *meshDataIterator++;
            if (numNormals > 0) /* Normals*/
            {
                for (p = 0; p < numNormals; p++)
                {
                    const int x = *meshDataIterator++;
                    const int y = *meshDataIterator++;
                    const int z = *meshDataIterator++;

                    /* Convert into a floating-point normal vector.*/
                    const float nx = (x / 16384.0);
                    const float ny = (y / 16384.0);
                    const float nz = (z / 16384.0);

                    /* Normals aren't exported anywhere at the moment, so let's just ignore them.*/
                    (void)nx;
                    (void)ny;
                    (void)nz;
                }
            }
            else /* Lights.*/
            {
                numNormals = abs(numNormals);
                for (p = 0; p < numNormals; p++)
                {
                    meshDataIterator++;
                }
            }

            #define LOAD_OBJECT_MESH_FACES(dstMeshArray, numFaces, numVertsPerFace)\
                    dstMeshArray = malloc(sizeof(struct tr_quad_s) * numFaces);\
                    for (p = 0; p < numFaces; p++)\
                    {\
                        unsigned v = 0;\
                        for (v = 0; v < numVertsPerFace; v++)\
                        {\
                            const unsigned vertexIdx = *meshDataIterator++;\
                            assert((vertexIdx < numVertices) && "Invalid vertex list index.");\
                            dstMeshArray[p].vertex[v] = vertexList[vertexIdx];\
                        }\
                        dstMeshArray[p].textureIdx = *meshDataIterator++;\
                    }

            objectMesh->numTexturedQuads = *meshDataIterator++;
            LOAD_OBJECT_MESH_FACES(objectMesh->texturedQuads, objectMesh->numTexturedQuads, 4);

            objectMesh->numTexturedTriangles = *meshDataIterator++;
            LOAD_OBJECT_MESH_FACES(objectMesh->texturedTriangles, objectMesh->numTexturedTriangles, 3);

            objectMesh->numUntexturedQuads = *meshDataIterator++;
            LOAD_OBJECT_MESH_FACES(objectMesh->untexturedQuads, objectMesh->numUntexturedQuads, 4);

            objectMesh->numUntexturedTriangles = *meshDataIterator++;
            LOAD_OBJECT_MESH_FACES(objectMesh->untexturedTriangles, objectMesh->numUntexturedTriangles, 3);

            #undef LOAD_OBJECT_MESH_FACES

            free(vertexList);
        }

        free(meshOffsets);
        free(meshData);
    }

    /* Read animations.*/
    {
        const unsigned numAnimations = read_value(4);
        print_file_pos(-4);printf(" Animations: %d\n", numAnimations);
        skip_num_bytes(SIZE_TR_ANIMATION * numAnimations);
    }

    /* Read state changes.*/
    {
        const unsigned numStateChanges = read_value(4);
        print_file_pos(-4);printf(" State changes: %d\n", numStateChanges);
        skip_num_bytes(SIZE_TR_STATE_CHANGE * numStateChanges);
    }

    /* Read animation dispatches.*/
    {
        const unsigned numAnimationDispatches = read_value(4);
        print_file_pos(-4);printf(" Animation dispatches: %d\n", numAnimationDispatches);
        skip_num_bytes(SIZE_TR_ANIM_DISPATCH * numAnimationDispatches);
    }

    /* Read animation commands.*/
    {
        const unsigned numAnimationCommands = read_value(4);
        print_file_pos(-4);printf(" Animation commands: %d\n", numAnimationCommands);
        skip_num_bytes(SIZE_TR_ANIM_COMMANDS * numAnimationCommands);
    }

    /* Read mesh trees.*/
    {
        const unsigned numMeshTrees = read_value(4);
        print_file_pos(-4);printf(" Mesh trees: %d\n", numMeshTrees);
        skip_num_bytes(SIZE_TR_MESH_TREE_NODE * numMeshTrees);
    }

    /* Read frames.*/
    {
        const unsigned numFrames = read_value(4);
        print_file_pos(-4);printf(" Frames: %d\n", numFrames);
        skip_num_bytes(numFrames * 2);
    }

    /* Read models.*/
    {
        const unsigned numModels = read_value(4);
        print_file_pos(-4);printf(" Models: %d\n", numModels);
        skip_num_bytes(SIZE_TR_MODEL * numModels);
    }

    /* Read static meshes.*/
    {
        const unsigned numStaticMeshes = read_value(4);
        print_file_pos(-4);printf(" Static meshes: %d\n", numStaticMeshes);
        for (i = 0; i < numStaticMeshes; i++)
        {
            const unsigned staticMeshId = read_value(4); /* A value identifying this static mesh.*/
            const unsigned meshIdx = read_value(2);      /* Index to the master list of meshes (IMPORTED_DATA.meshes).*/
            skip_num_bytes(12); /* Skip 'visibilityBox'.*/
            skip_num_bytes(12); /* Skip 'collisionBox'. */
            skip_num_bytes(2);  /* Skip 'flags'.        */

            /* Route the master mesh index information directly to the room's static
             * objects. Normally, the static objects have an index referring to this
             * metadata, which then refers to the master mesh list.*/
            for (p = 0; p < IMPORTED_DATA.numRoomMeshes; p++)
            {
                unsigned k = 0;

                for (k = 0; k < IMPORTED_DATA.roomMeshes[p].numStaticObjects; k++)
                {
                    if (IMPORTED_DATA.roomMeshes[p].staticObjects[k].meshIdx == staticMeshId)
                    {
                        IMPORTED_DATA.roomMeshes[p].staticObjects[k].meshIdx = meshIdx;
                    }
                }
            }
        }
    }

    /* Read object texture metadata.*/
    {
        IMPORTED_DATA.numObjectTextures = read_value(4);
        print_file_pos(-4);printf(" Object textures: %d\n", IMPORTED_DATA.numObjectTextures);

        IMPORTED_DATA.objectTextures = malloc(sizeof(struct tr_object_texture_s) * IMPORTED_DATA.numObjectTextures);

        for (i = 0; i < IMPORTED_DATA.numObjectTextures; i++)
        {
            struct tr_object_texture_s *const texture = &IMPORTED_DATA.objectTextures[i];
            unsigned textureAtlasIdx = 0;
            unsigned isTriangle = 0;
            unsigned attribute = 0;

            attribute = read_value(2);
            textureAtlasIdx = read_value(2);
            isTriangle = (textureAtlasIdx & 0x1);
            textureAtlasIdx = (textureAtlasIdx & 0x7fff);
            texture->hasAlpha = ((attribute == 1) || (attribute == 4));
            texture->ignoresDepthTest = (attribute == 4);
            texture->hasWireframe = (attribute == 6);

            assert((textureAtlasIdx < IMPORTED_DATA.numTextureAtlases) && "Texture atlas index out of bounds.");

            /* Copy the texture's data from the texture atlas.*/
            {
                unsigned minX = ~0u, maxX = 0, minY = ~0u, maxY = 0;
                unsigned cornerPoints[4][2] = {0}; /* 4 texel coordinate pairs defining this texture's rectangle in the texture atlas.*/

                for (p = 0; p < 4; p++)
                {
                    const unsigned x = read_value(2);
                    const unsigned y = read_value(2);

                    cornerPoints[p][0] = (unsigned)((x & 0xff00) / 256.0);
                    cornerPoints[p][1] = (unsigned)((y & 0xff00) / 256.0);

                    texture->u[3-p] = ((x & 0xff) / 256.0);
                    texture->v[3-p] = ((y & 0xff) / 256.0);
                }

                for (p = 0; p < (isTriangle? 3 : 4); p++)
                {
                    if (cornerPoints[p][0] > maxX) maxX = cornerPoints[p][0];
                    if (cornerPoints[p][0] < minX) minX = cornerPoints[p][0];

                    if (cornerPoints[p][1] > maxY) maxY = cornerPoints[p][1];
                    if (cornerPoints[p][1] < minY) minY = cornerPoints[p][1];
                }
                
                texture->width = ((maxX - minX) + 1);
                texture->height = ((maxY - minY) + 1);
                texture->pixelData = malloc(texture->width * texture->height);

                /* Copy the pixel data row by row.*/
                for (p = 0; p < texture->height; p++)
                {
                    const unsigned srcIdx = (minX + (minY + p) * IMPORTED_DATA.textureAtlases[textureAtlasIdx].width);
                    const unsigned dstIdx = (p * texture->width);

                    memcpy((texture->pixelData + dstIdx),
                           (IMPORTED_DATA.textureAtlases[textureAtlasIdx].pixelData + srcIdx),
                           texture->width);
                }
            }
        }
    }

    /* Read sprite textures.*/
    {
        const unsigned numSpriteTextures = read_value(4);
        print_file_pos(-4);printf(" Sprite textures: %d\n", numSpriteTextures);
        skip_num_bytes(SIZE_TR_SPRITE_TEXTURE * numSpriteTextures);
    }

    /* Read sprite sequences.*/
    {
        const unsigned numSpriteSequences = read_value(4);
        print_file_pos(-4);printf(" Sprite sequences: %d\n", numSpriteSequences);
        skip_num_bytes(SIZE_TR_SPRITE_SEQUENCE * numSpriteSequences);
    }

    /* Read cameras.*/
    {
        const unsigned numCameras = read_value(4);
        print_file_pos(-4);printf(" Cameras: %d\n", numCameras);
        skip_num_bytes(SIZE_TR_CAMERA * numCameras);
    }

    /* Read sound sources.*/
    {
        const unsigned numSoundSources = read_value(4);
        print_file_pos(-4);printf(" Sound sources: %d\n", numSoundSources);
        skip_num_bytes(SIZE_TR_SOUND_SOURCE * numSoundSources);
    }

    /* Read boxes and overlaps.*/
    {
        unsigned numBoxes = 0;
        unsigned numOverlaps = 0;
        
        numBoxes = read_value(4);
        print_file_pos(-4);printf(" Boxes: %d\n", numBoxes);
        skip_num_bytes(SIZE_TR_BOX * numBoxes);

        numOverlaps = read_value(4);
        print_file_pos(-4);printf(" Overlaps: %d\n", numBoxes);
        skip_num_bytes(numOverlaps * 2);

        skip_num_bytes(numBoxes * 2); /* groundZone     */
        skip_num_bytes(numBoxes * 2); /* groundZone2    */
        skip_num_bytes(numBoxes * 2); /* flyZone        */
        skip_num_bytes(numBoxes * 2); /* groundZoneAlt  */
        skip_num_bytes(numBoxes * 2); /* groundZoneAlt2 */
        skip_num_bytes(numBoxes * 2); /* flyZoneAlt     */
    }

    /* Read animated textures.*/
    {
        const unsigned numAnimatedTextures = read_value(4);
        print_file_pos(-4);printf(" Animated textures: %d\n", numAnimatedTextures);
        skip_num_bytes(numAnimatedTextures * 2);
    }

    /* Read entities.*/
    {
        const unsigned numEntities = read_value(4);
        print_file_pos(-4);printf(" Entities: %d\n", numEntities);
        skip_num_bytes(SIZE_TR_ENTITY * numEntities);
    }

    /* Read lightmap.*/
    {
        skip_num_bytes(8192);
    }

    /* Read palette.*/
    {
        IMPORTED_DATA.palette = malloc(768);
        read_bytes((char*)IMPORTED_DATA.palette, 768);

        for (i = 0; i < 768; i++)
        {
            /* Convert colors from VGA 6-bit to full 8-bit.*/
            IMPORTED_DATA.palette[i] = (IMPORTED_DATA.palette[i] * 4);
        }
    }

    /* Read cinematic frames.*/
    {
        const unsigned numCinematicFrames = read_value(2);
        print_file_pos(-2);printf(" Cinematic frames: %d\n", numCinematicFrames);
        skip_num_bytes(SIZE_TR_CINEMATIC_FRAME * numCinematicFrames);
    }

    /* Read demo data.*/
    {
        const unsigned numDemoData = read_value(2);
        print_file_pos(-2);printf(" Demo data: %d\n", numDemoData);
        skip_num_bytes(numDemoData);
    }

    /* Skip the rest of the file (sound data).*/
    /* Skip the rest of the file (sound data).*/
    /* Skip the rest of the file (sound data).*/

    return;
}

void export_imported_data(void)
{
    int i = 0, p = 0;

    /* Save the level's palette.*/
    {
        FILE *outFile = fopen("output/texture/palette.pal", "wb");
        assert(outFile && "Failed to open an output file for exporting the palette.");

        fwrite((char*)IMPORTED_DATA.palette, 1, 768, outFile);

        fclose(outFile);
    }

    /* Save textures.*/
    {
        #define SAVE_TEXTURE(path, texture)\
                FILE *outFile, *metaFile;\
                char filename[256];\
                const unsigned numPixels = (texture.width * texture.height);\
                \
                sprintf(filename, "%s%d.trt", path, i);\
                outFile = fopen(filename, "wb");\
                \
                sprintf(filename, "%s%d.trt.mta", path, i);\
                metaFile = fopen(filename, "wb");\
                \
                assert((outFile && metaFile) && "Failed to open an output file to export a texture into.");\
                \
                fwrite((char*)texture.pixelData, 1, numPixels, outFile);\
                \
                fprintf(metaFile, "%d %d", texture.width, texture.height);\
                \
                fclose(outFile);\
                fclose(metaFile);\

        /* Save the texture atlases.*/
        {
            for (i = 0; i < IMPORTED_DATA.numTextureAtlases; i++)
            {
                SAVE_TEXTURE("output/texture/atlas/", IMPORTED_DATA.textureAtlases[i]);
            }
        }

        /* Save the object textures.*/
        {
            for (i = 0; i < IMPORTED_DATA.numObjectTextures; i++)
            {
                SAVE_TEXTURE("output/texture/object/", IMPORTED_DATA.objectTextures[i]);
            }
        }

        #undef SAVE_TEXTURE
    }

    /* Save the room meshes.*/
    {
        #define SAVE_ROOM_FACES(numFaces, faceData, numVertsPerFace, facesAreTextured) \
                for (j = 0; j < numFaces; j++)\
                {\
                    int v = 0;\
                    \
                    sprintf(tmp, "%d %d", numVertsPerFace, (facesAreTextured? faceData[j].textureIdx : -(faceData[j].textureIdx & 0xff)));\
                    fputs(tmp, outFile);\
                    \
                    for (v = 0; v < numVertsPerFace; v++)\
                    {\
                        sprintf(tmp, " %d %d %d %f %f", faceData[j].vertex[v].x,\
                                                        faceData[j].vertex[v].y,\
                                                        faceData[j].vertex[v].z,\
                                                        IMPORTED_DATA.objectTextures[faceData[j].textureIdx].u[v],\
                                                        IMPORTED_DATA.objectTextures[faceData[j].textureIdx].v[v]);\
                        \
                        fputs(tmp, outFile);\
                    }\
                    \
                    fputs("\n", outFile);\
                }\

        #define SAVE_ROOM_OBJECT_FACES(numFaces, faceData, metaData, numVertsPerFace, facesAreTextured) \
                for (j = 0; j < numFaces; j++)\
                {\
                    int v = 0;\
                    \
                    sprintf(tmp, "%d %d", numVertsPerFace, (facesAreTextured? faceData[j].textureIdx : -(faceData[j].textureIdx & 0xff)));\
                    fputs(tmp, outFile);\
                    \
                    for (v = 0; v < numVertsPerFace; v++)\
                    {\
                        int r = 0;\
                        \
                        int x = faceData[j].vertex[v].x;\
                        int y = faceData[j].vertex[v].y;\
                        int z = faceData[j].vertex[v].z;\
                        \
                        /* Rotate the vertex.*/\
                        for (r = 0; r < metaData->rotation; r++)\
                        {\
                            int tmp = x;\
                            x = z;\
                            z = -tmp;\
                        }\
                        \
                        sprintf(tmp, " %d %d %d %f %f", (x + metaData->x),\
                                                        (y + metaData->y),\
                                                        (z + metaData->z),\
                                                        IMPORTED_DATA.objectTextures[faceData[j].textureIdx].u[v],\
                                                        IMPORTED_DATA.objectTextures[faceData[j].textureIdx].v[v]);\
                        \
                        fputs(tmp, outFile);\
                    }\
                    \
                    fputs("\n", outFile);\
                }\
                
        for (i = 0; i < IMPORTED_DATA.numRoomMeshes; i++)
        {
            unsigned j = 0;
            FILE *outFile;
            char tmp[256];
            char meshFileName[256];

            sprintf(meshFileName, "output/mesh/room/%d.trm", i);

            outFile = fopen(meshFileName, "wb");
            assert(outFile && "Failed to open an output file to export a mesh into.");

            /* Save the room's mesh.*/
            SAVE_ROOM_FACES(IMPORTED_DATA.roomMeshes[i].numQuads, IMPORTED_DATA.roomMeshes[i].quads, 4, 1);
            SAVE_ROOM_FACES(IMPORTED_DATA.roomMeshes[i].numTriangles, IMPORTED_DATA.roomMeshes[i].triangles, 3, 1);

            /* Save the room's static objects' meshes.*/
            for (p = 0; p < IMPORTED_DATA.roomMeshes[i].numStaticObjects; p++)
            {
                const struct tr_mesh_meta_s *const objectMeta = &IMPORTED_DATA.roomMeshes[i].staticObjects[p];
                const struct tr_mesh_s *const object = &IMPORTED_DATA.meshes[objectMeta->meshIdx];

                SAVE_ROOM_OBJECT_FACES(object->numTexturedQuads, object->texturedQuads, objectMeta, 4, 1);
                SAVE_ROOM_OBJECT_FACES(object->numTexturedTriangles, object->texturedTriangles, objectMeta, 3, 1);
                SAVE_ROOM_OBJECT_FACES(object->numUntexturedQuads, object->untexturedQuads, objectMeta, 4, 0);
                SAVE_ROOM_OBJECT_FACES(object->numUntexturedTriangles, object->untexturedTriangles, objectMeta, 3, 0);
            }
        }

        #undef SAVE_ROOM_FACES
        #undef SAVE_ROOM_OBJECT_FACES
    }

    return;
}

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        printf("Usage: %s <PHD filename>\n", argv[0]);
        return 1;
    }
    
    INPUT_FILE = fopen(argv[1], "rb");
    assert(INPUT_FILE && "Could not open the PHD file.");

    import_data_from_input_file();
    export_imported_data();

    return 0;
}
