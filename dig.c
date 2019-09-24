/*
 * 2019 Tarpeeksi Hyvae Soft
 * dig
 * 
 * Provides (partial) access to Tomb Raider 1's .PHD level files.
 * 
 * Based on the third-party file format documentation available at
 * https://trwiki.earvillage.net/doku.php?id=trs:file_formats.
 * 
 * Note that this project is a simple tool to scratch a personal itch. For
 * purposes of modding and the like, there surely exist various other more
 * relevant solutions.
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
#define SIZE_TR_BOX 20

struct tr_object_texture_s
{
    unsigned width, height;
    unsigned hasAlpha;
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
    int textureIdx;
};

struct tr_triangle_s
{
    struct tr_vertex_s vertex[3];
    int textureIdx;
};

struct tr_room_mesh_s
{
    unsigned numQuads;
    struct tr_quad_s *quads;

    unsigned numTriangles;
    struct tr_triangle_s *triangles;
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

void import(void)
{
    int i = 0, p = 0;

    IMPORTED_DATA.fileVersion = (uint32_t)read_value(4);

    assert((IMPORTED_DATA.fileVersion == 32) && "Expected a Tomb Raider 1 level file.");

    /* Read textures.*/
    {
        IMPORTED_DATA.numTextureAtlases = (uint32_t)read_value(4);

        IMPORTED_DATA.textureAtlases = malloc(sizeof(struct tr_texture_atlas_s));

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
        IMPORTED_DATA.numRoomMeshes = (uint16_t)read_value(2);
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
            unsigned numStaticMeshes = 0;
            unsigned alternateRoom = 0;
            int16_t flags = 0;

            print_file_pos(0);printf("   #%d\n", i);

            /* Room info.*/
            skip_num_bytes(SIZE_TR_ROOM_INFO);

            /* Room data.*/
            {
                char *rawRoomMeshData = NULL;

                numRoomDataWords = (uint32_t)read_value(4);
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
                            vertexList[p].x = *roomDataIterator++;
                            vertexList[p].y = *roomDataIterator++;
                            vertexList[p].z = *roomDataIterator++;
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
                        }
                    }
                }
            }

            /* Portals.*/
            numPortals = (uint16_t)read_value(2);
            print_file_pos(-2);printf("     Portals: %d\n", numPortals);
            skip_num_bytes(SIZE_TR_ROOM_PORTAL * numPortals);

            /* Sectors.*/
            numZSectors = (uint16_t)read_value(2);
            numXSectors = (uint16_t)read_value(2);
            print_file_pos(-4);printf("     Sectors: %d, %d\n", numZSectors, numXSectors);
            skip_num_bytes(SIZE_TR_ROOM_SECTOR * numZSectors * numXSectors);

            /* Lights.*/
            ambientIntensity = (int16_t)read_value(2);
            numLights = (uint16_t)read_value(2);
            print_file_pos(-4);printf("     Lights: %d (%d)\n", numLights, ambientIntensity);
            skip_num_bytes(SIZE_TR_ROOM_LIGHT * numLights);

            /* Static meshes.*/
            numStaticMeshes = (uint16_t)read_value(2);
            print_file_pos(-2);printf("     Static meshes: %d\n", numStaticMeshes);
            skip_num_bytes(SIZE_TR_ROOM_STATIC_MESH * numStaticMeshes);

            /* Miscellaneous.*/
            alternateRoom = (uint16_t)read_value(2);
            flags = (uint16_t)read_value(2);
            print_file_pos(-2);printf("     Alternate room: %d\n", alternateRoom);
            print_file_pos(-2);printf("     Flags: 0x%x\n", flags);
        }
    }

    /* Read floors.*/
    {
        const unsigned numFloors = (uint32_t)read_value(4);
        skip_num_bytes(numFloors * 2);
    }

    /* Read meshes.*/
    {
        unsigned numMeshDataWords = 0;
        unsigned numMeshPointers = 0;

        numMeshDataWords = (uint32_t)read_value(4);
        skip_num_bytes(numMeshDataWords * 2);

        numMeshPointers = (uint32_t)read_value(4);
        print_file_pos(-4);printf(" Mesh pointers: %d\n", numMeshPointers);
        skip_num_bytes(numMeshPointers * 4);
    }

    /* Read animations.*/
    {
        const unsigned numAnimations = (uint32_t)read_value(4);
        print_file_pos(-4);printf(" Animations: %d\n", numAnimations);
        skip_num_bytes(SIZE_TR_ANIMATION * numAnimations);
    }

    /* Read state changes.*/
    {
        const unsigned numStateChanges = (uint32_t)read_value(4);
        print_file_pos(-4);printf(" State changes: %d\n", numStateChanges);
        skip_num_bytes(SIZE_TR_STATE_CHANGE * numStateChanges);
    }

    /* Read animation dispatches.*/
    {
        const unsigned numAnimationDispatches = (uint32_t)read_value(4);
        print_file_pos(-4);printf(" Animation dispatches: %d\n", numAnimationDispatches);
        skip_num_bytes(SIZE_TR_ANIM_DISPATCH * numAnimationDispatches);
    }

    /* Read animation commands.*/
    {
        const unsigned numAnimationCommands = (uint32_t)read_value(4);
        print_file_pos(-4);printf(" Animation commands: %d\n", numAnimationCommands);
        skip_num_bytes(SIZE_TR_ANIM_COMMANDS * numAnimationCommands);
    }

    /* Read mesh trees.*/
    {
        const unsigned numMeshTrees = (uint32_t)read_value(4);
        print_file_pos(-4);printf(" Mesh trees: %d\n", numMeshTrees);
        skip_num_bytes(SIZE_TR_MESH_TREE_NODE * numMeshTrees);
    }

    /* Read frames.*/
    {
        const unsigned numFrames = (uint32_t)read_value(4);
        print_file_pos(-4);printf(" Frames: %d\n", numFrames);
        skip_num_bytes(numFrames * 2);
    }

    /* Read models.*/
    {
        const unsigned numModels = (uint32_t)read_value(4);
        print_file_pos(-4);printf(" Models: %d\n", numModels);
        skip_num_bytes(SIZE_TR_MODEL * numModels);
    }

    /* Read static meshes.*/
    {
        const unsigned numStaticMeshes = (uint32_t)read_value(4);
        print_file_pos(-4);printf(" Static meshes: %d\n", numStaticMeshes);
        skip_num_bytes(SIZE_TR_STATIC_MESH * numStaticMeshes);
    }

    /* Read object texture metadata.*/
    {
        IMPORTED_DATA.numObjectTextures = (uint32_t)read_value(4);
        print_file_pos(-4);printf(" Object textures: %d\n", IMPORTED_DATA.numObjectTextures);

        IMPORTED_DATA.objectTextures = malloc(sizeof(struct tr_object_texture_s) * IMPORTED_DATA.numObjectTextures);

        for (i = 0; i < IMPORTED_DATA.numObjectTextures; i++)
        {
            struct tr_object_texture_s *const texture = &IMPORTED_DATA.objectTextures[i];
            unsigned textureAtlasIdx = 0;
            unsigned isTriangle = 0;

            texture->hasAlpha = (uint16_t)read_value(2);
            textureAtlasIdx = (uint16_t)read_value(2);
            isTriangle = (textureAtlasIdx & 0x1);
            textureAtlasIdx = (textureAtlasIdx & 0x7fff);

            assert((textureAtlasIdx < IMPORTED_DATA.numTextureAtlases) && "Texture atlas index out of bounds.");

            /* Copy the texture's data from the texture atlas.*/
            {
                unsigned minX = ~0u, maxX = 0, minY = ~0u, maxY = 0;
                unsigned cornerPoints[4][2] = {0}; /* 4 uv coordinate pairs defining this texture's rectangle in the texture atlas.*/

                for (p = 0; p < 4; p++)
                {
                    cornerPoints[p][0] = (unsigned)((uint16_t)read_value(2) / (float)(1 << 8));
                    cornerPoints[p][1] = (unsigned)((uint16_t)read_value(2) / (float)(1 << 8));
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
        const unsigned numSpriteTextures = (uint32_t)read_value(4);
        print_file_pos(-4);printf(" Sprite textures: %d\n", numSpriteTextures);
        skip_num_bytes(SIZE_TR_SPRITE_TEXTURE * numSpriteTextures);
    }

    /* Read sprite sequences.*/
    {
        const unsigned numSpriteSequences = (uint32_t)read_value(4);
        print_file_pos(-4);printf(" Sprite sequences: %d\n", numSpriteSequences);
        skip_num_bytes(SIZE_TR_SPRITE_SEQUENCE * numSpriteSequences);
    }

    /* Read cameras.*/
    {
        const unsigned numCameras = (uint32_t)read_value(4);
        print_file_pos(-4);printf(" Cameras: %d\n", numCameras);
        skip_num_bytes(SIZE_TR_CAMERA * numCameras);
    }

    /* Read sound sources.*/
    {
        const unsigned numSoundSources = (uint32_t)read_value(4);
        print_file_pos(-4);printf(" Sound sources: %d\n", numSoundSources);
        skip_num_bytes(SIZE_TR_SOUND_SOURCE * numSoundSources);
    }

    /* Read sound boxes and overlaps.*/
    {
        unsigned numBoxes = 0;
        unsigned numOverlaps = 0;
        
        numBoxes = (uint32_t)read_value(4);
        print_file_pos(-4);printf(" Boxes: %d\n", numBoxes);
        skip_num_bytes(SIZE_TR_BOX * numBoxes);

        numOverlaps = (uint32_t)read_value(4);
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
        const unsigned numAnimatedTextures = (uint32_t)read_value(4);
        print_file_pos(-4);printf(" Animated textures: %d\n", numAnimatedTextures);
        skip_num_bytes(numAnimatedTextures * 2);
    }

    /* Read entities.*/
    {
        const unsigned numEntities = (uint32_t)read_value(4);
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
        const unsigned numCinematicFrames = (uint16_t)read_value(2);
        print_file_pos(-2);printf(" Cinematic frames: %d\n", numCinematicFrames);
        skip_num_bytes(SIZE_TR_CINEMATIC_FRAME * numCinematicFrames);
    }

    /* Read demo data.*/
    {
        const unsigned numDemoData = (uint16_t)read_value(2);
        print_file_pos(-2);printf(" Demo data: %d\n", numDemoData);
        skip_num_bytes(numDemoData);
    }

    /* Skip the rest of the file (sound data).*/
    /* Skip the rest of the file (sound data).*/
    /* Skip the rest of the file (sound data).*/

    return;
}

void export(void)
{
    int i = 0, p = 0;

    /* Save the level's palette.*/
    {
        FILE *outFile = fopen("output/texture/palette.pal", "wb");
        assert(outFile && "Failed to open a file to save the palette into.");

        fwrite((char*)IMPORTED_DATA.palette, 1, 768, outFile);

        fclose(outFile);
    }

    /* Save the texture atlases.*/
    {
        for (i = 0; i < IMPORTED_DATA.numTextureAtlases; i++)
        {
            FILE *outFile, *metaFile;
            char tmp[256];
            const unsigned numPixels = (IMPORTED_DATA.textureAtlases[i].width * IMPORTED_DATA.textureAtlases[i].height);

            sprintf(tmp, "output/texture/atlas/%d.trt", i);
            outFile = fopen(tmp, "wb");

            sprintf(tmp, "output/texture/atlas/%d.trt.mta", i);
            metaFile = fopen(tmp, "wb");

            assert((outFile && metaFile) && "Failed to open a file to save a texture into.");

            fwrite((char*)IMPORTED_DATA.textureAtlases[i].pixelData, 1, numPixels, outFile);

            sprintf(tmp, "%d %d", IMPORTED_DATA.textureAtlases[i].width,
                                  IMPORTED_DATA.textureAtlases[i].height);
            fputs(tmp, metaFile);

            fclose(outFile);
            fclose(metaFile);
        }
    }

    /* Save the object textures.*/
    {
        for (i = 0; i < IMPORTED_DATA.numObjectTextures; i++)
        {
            FILE *outFile, *metaFile;
            char tmp[128];
            const unsigned numPixels = (IMPORTED_DATA.objectTextures[i].width * IMPORTED_DATA.objectTextures[i].height);

            sprintf(tmp, "output/texture/object/%d.trt", i);
            outFile = fopen(tmp, "wb");

            sprintf(tmp, "output/texture/object/%d.trt.mta", i);
            metaFile = fopen(tmp, "wb");

            assert((outFile && metaFile) && "Failed to open a file to save a texture into.");

            fwrite((char*)IMPORTED_DATA.objectTextures[i].pixelData, 1, numPixels, outFile);

            sprintf(tmp, "%d %d", IMPORTED_DATA.objectTextures[i].width,
                                  IMPORTED_DATA.objectTextures[i].height);
            fputs(tmp, metaFile);

            fclose(outFile);
            fclose(metaFile);
        }
    }

    /* Save the room mesh.*/
    {
        for (i = 0; i < IMPORTED_DATA.numRoomMeshes; i++)
        {
            FILE *outFile;
            char tmp[256];
            char meshFileName[256];

            sprintf(meshFileName, "output/mesh/room/%d.trm", i);

            outFile = fopen(meshFileName, "wb");
            assert(outFile && "Failed to open a file to save a mesh into.");

            /* Save the quads.*/
            for (p = 0; p < IMPORTED_DATA.roomMeshes[i].numQuads; p++)
            {
                int v = 0;

                /* Four vertices per face.*/
                sprintf(tmp, "4 %d", IMPORTED_DATA.roomMeshes[i].quads[p].textureIdx);
                fputs(tmp, outFile);

                for (v = 0; v < 4; v++)
                {
                    sprintf(tmp, " %d %d %d", IMPORTED_DATA.roomMeshes[i].quads[p].vertex[v].x,
                                              IMPORTED_DATA.roomMeshes[i].quads[p].vertex[v].y,
                                              IMPORTED_DATA.roomMeshes[i].quads[p].vertex[v].z);

                    fputs(tmp, outFile);
                }

                fputs("\n", outFile);
            }

            /* Save the triangles.*/
            for (p = 0; p < IMPORTED_DATA.roomMeshes[i].numTriangles; p++)
            {
                int v = 0;

                /* Three vertices per face.*/
                sprintf(tmp, "3 %d", IMPORTED_DATA.roomMeshes[i].triangles[p].textureIdx);
                fputs(tmp, outFile);

                for (v = 0; v < 3; v++)
                {
                    sprintf(tmp, " %d %d %d", IMPORTED_DATA.roomMeshes[i].triangles[p].vertex[v].x,
                                              IMPORTED_DATA.roomMeshes[i].triangles[p].vertex[v].y,
                                              IMPORTED_DATA.roomMeshes[i].triangles[p].vertex[v].z);

                    fputs(tmp, outFile);
                }

                fputs("\n", outFile);
            }
        }
    }

    return;
}

int main(void)
{
    INPUT_FILE = fopen("level/1.phd", "rb");

    assert(INPUT_FILE && "Failed to open the level file for reading.");

    import();
    export();

    return 0;
}
