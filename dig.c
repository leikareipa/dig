#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>

/* Placeholder byte sizes of various Tomb Raider data structs.*/
#define SIZE_TR_ROOM_STATIC_MESH 18
#define SIZE_TR_CINEMATIC_FRAME 16
#define SIZE_TR_OBJECT_TEXTURE 20
#define SIZE_TR_SPRITE_TEXTURE 16
#define SIZE_TR_SPRITE_SEQUENCE 8
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

struct tr_texture_atlas_s
{
    unsigned width, height;
    uint8_t *pixels;
};

struct phd_file_contents_s
{
    unsigned fileVersion;

    unsigned numTextures;
    struct tr_texture_atlas_s *textureAtlases;

    uint8_t *palette;
};

static FILE *INPUT_FILE;
static struct phd_file_contents_s IMPORTED;

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
    int i = 0;

    IMPORTED.fileVersion = (uint32_t)read_value(4);

    assert((IMPORTED.fileVersion == 32) && "Unsupported file format.");

    /* Read textures.*/
    {
        IMPORTED.numTextures = (uint32_t)read_value(4);

        IMPORTED.textureAtlases = malloc(sizeof(struct tr_texture_atlas_s));

        for (i = 0; i < IMPORTED.numTextures; i++)
        {
            unsigned numPixels;

            IMPORTED.textureAtlases[i].width = 256;
            IMPORTED.textureAtlases[i].height = 256;
            numPixels = (IMPORTED.textureAtlases[i].width * IMPORTED.textureAtlases[i].height);

            IMPORTED.textureAtlases[i].pixels = malloc(numPixels);
            read_bytes((char*)IMPORTED.textureAtlases[i].pixels, numPixels);
        }
    }

    /* Skip unknown dword.*/
    skip_num_bytes(4);

    /* Read rooms.*/
    {
        const unsigned numRooms = (uint16_t)read_value(2);

        print_file_pos(-2);
        printf(" Rooms: %d\n", numRooms);

        for (i = 0; i < numRooms; i++)
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

            /* Info room info.*/
            skip_num_bytes(SIZE_TR_ROOM_INFO);

            /* Room data.*/
            numRoomDataWords = (uint32_t)read_value(4);
            skip_num_bytes(numRoomDataWords * 2);

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

    /* Read object textures.*/
    {
        const unsigned numObjectTextures = (uint32_t)read_value(4);
        print_file_pos(-4);printf(" Object textures: %d\n", numObjectTextures);
        skip_num_bytes(SIZE_TR_OBJECT_TEXTURE * numObjectTextures);
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
        IMPORTED.palette = malloc(768);
        read_bytes((char*)IMPORTED.palette, 768);

        for (i = 0; i < 768; i++)
        {
            /* Convert colors from VGA 6-bit to full 8-bit.*/
            IMPORTED.palette[i] = (IMPORTED.palette[i] * 4);
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
    int i = 0;

    /* Save the level's palette.*/
    {
        FILE *outFile = fopen("output/texture/atlas/palette.pal", "wb");

        assert(outFile && "Failed to open a file to save the palette into.");

        fwrite((char*)IMPORTED.palette, 1, 768, outFile);

        fclose(outFile);
    }

    /* Save the texture atlases.*/
    {
        for (i = 0; i < IMPORTED.numTextures; i++)
        {
            FILE *outFile;
            char textureName[128];
            const unsigned numPixels = (IMPORTED.textureAtlases[i].width * IMPORTED.textureAtlases[i].height);

            sprintf(textureName, "output/texture/atlas/%d.trt", i);

            outFile = fopen(textureName, "wb");

            assert(outFile && "Failed to open a file to save a texture into.");

            fwrite((char*)IMPORTED.textureAtlases[i].pixels, 1, numPixels, outFile);

            fclose(outFile);
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
