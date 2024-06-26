#include "Map.hpp"

Map::Map(flecs::world *ecs_world)
{
    //--------------------------------------------------------------------------------------
    // Set ECS World for adding map objects
    //--------------------------------------------------------------------------------------
    this->ecs_world = ecs_world;

    //--------------------------------------------------------------------------------------
    // Parse Map
    //--------------------------------------------------------------------------------------
    map = cute_tiled_load_map_from_file("speedjam5map.json", NULL);

    //--------------------------------------------------------------------------------------
    // Load Tileset Textures
    //--------------------------------------------------------------------------------------
    cute_tiled_tileset_t *ts_ptr = map->tilesets;
    while (ts_ptr)
    {
        // Get the tileset image's path
        std::filesystem::path ts_path(ts_ptr->image.ptr);

        // Load tileset's image
        Image ts_img = LoadImage(ts_path.filename().string().c_str());
        TilesetInfo ts_info;
        ts_info.info = *ts_ptr;

        // Load texture
        ts_info.tex = LoadTextureFromImage(ts_img);
        UnloadImage(ts_img);

        // Add to tilesets
        tilesets_info.push_back(ts_info);

        // Go onto next tileset
        ts_ptr = ts_ptr->next;
    }

    //--------------------------------------------------------------------------------------
    // Get map basic data
    //--------------------------------------------------------------------------------------
    int map_w = map->width;
    int map_h = map->height;

    int tile_w = map->tilewidth;
    int tile_h = map->tileheight;

    //--------------------------------------------------------------------------------------
    // Initialize map rendertarget
    //--------------------------------------------------------------------------------------
    map_target = LoadRenderTexture(map_w * tile_w, map_h * tile_h);
    map_target_front = LoadRenderTexture(map_w * tile_w, map_h * tile_h);

    //--------------------------------------------------------------------------------------
    // Render map layers to RenderTexture
    //--------------------------------------------------------------------------------------

    cute_tiled_layer_t *layer = map->layers;

    while (layer)
    {
        if (std::string("tilelayer") == layer->type.ptr)
        {
            int *data = layer->data;
            int data_count = layer->data_count;

            for (int column = 0; column < map_w; column++)
            {
                for (int row = 0; row < map_h; row++)
                {
                    // Get the tile num for the tile on this layer
                    int tile_data = data[map_w * row + column];

                    if (tile_data == 0)
                        continue;

                    // Determine the tile's tileset
                    TilesetInfo *this_tile_info;

                    for (auto &tile_info : tilesets_info)
                    {
                        if (tile_info.info.firstgid <= tile_data && tile_data <= tile_info.info.firstgid + tile_info.info.tilecount - 1)
                        {
                            this_tile_info = &tile_info;
                            break;
                        }
                    }

                    // Draw to the rendertexture
                    Rectangle src_rect = {(float)tile_w * ((tile_data - this_tile_info->info.firstgid) % this_tile_info->info.columns),
                                          (float)tile_h * ((tile_data - this_tile_info->info.firstgid) / this_tile_info->info.columns),
                                          (float)tile_w,
                                          (float)tile_h};

                    Vector2 dest_pos = {(float)column * tile_w, (float)row * tile_h};
                    
                    if (std::string("frontlayer") == layer->class_.ptr)
                        BeginTextureMode(map_target_front);
                    else
                        BeginTextureMode(map_target);

                    DrawTextureRec(this_tile_info->tex, src_rect, dest_pos, WHITE);
                    EndTextureMode();
                }
            }
        }
        else if (std::string("objectgroup") == layer->type.ptr)
        {
            //--------------------------------------------------------------------------------------
            // Add Solid Bodies
            //--------------------------------------------------------------------------------------
            if (std::string("Collision") == layer->name.ptr)
            {
                cute_tiled_object_t *layer_obj = layer->objects;

                while (layer_obj)
                {
                    flecs::entity solid_e = ecs_world->entity();
                    solid_e.set<plt::Position>({layer_obj->x, layer_obj->y});
                    solid_e.set<plt::Collider>({Rectangle{0, 0, layer_obj->width, layer_obj->height}, c2AABB{0, 0, 0, 0}});
                    solid_e.set<plt::SolidBody>({1});

                    layer_obj = layer_obj->next;
                }
            }

            //--------------------------------------------------------------------------------------
            // Add objects
            //--------------------------------------------------------------------------------------
            else if (std::string("Objects") == layer->name.ptr)
            {
                cute_tiled_object_t *layer_obj = layer->objects;

                while (layer_obj)
                {
                    // ==================================================
                    // Add the player at spawn
                    // ==================================================
                    if (std::string("Spawn") == layer_obj->name.ptr)
                    {
                        flecs::entity player_e = ecs_world->entity();
                        player_e.set<plt::Position>({layer_obj->x, layer_obj->y});
                        player_e.set<plt::Player>({false, plt::PlayerMvnmtState_Forward, 0, 0.1, 0.3, 0, flecs::Empty, plt::PlayerHoldingType_None, plt::CookingZone_None});
                        player_e.set<plt::Collider>({Rectangle{-7, -2, 14, 8}, c2AABB{0, 0, 0, 0}});
                        player_e.set<plt::DynamicBody>({1});
                    }
                    else if (std::string("Bag") == layer_obj->name.ptr)
                    {
                        flecs::entity zone_e = ecs_world->entity();
                        zone_e.set<plt::CookingZone>({plt::CookingZone_Bag, Rectangle{layer_obj->x, layer_obj->y, layer_obj->width, layer_obj->height}});
                    }
                    else if (std::string("Dishes") == layer_obj->name.ptr)
                    {
                        flecs::entity zone_e = ecs_world->entity();
                        zone_e.set<plt::CookingZone>({plt::CookingZone_Dishes, Rectangle{layer_obj->x, layer_obj->y, layer_obj->width, layer_obj->height}});
                    }
                    else if (std::string("Sink") == layer_obj->name.ptr)
                    {
                        flecs::entity zone_e = ecs_world->entity();
                        zone_e.set<plt::CookingZone>({plt::CookingZone_Sink, Rectangle{layer_obj->x, layer_obj->y, layer_obj->width, layer_obj->height}});
                    }
                    else if (std::string("CuttingBoard") == layer_obj->name.ptr)
                    {
                        flecs::entity zone_e = ecs_world->entity();
                        zone_e.set<plt::CookingZone>({plt::CookingZone_CuttingBoard, Rectangle{layer_obj->x, layer_obj->y, layer_obj->width, layer_obj->height}});
                    }
                    else if (std::string("Stove") == layer_obj->name.ptr)
                    {
                        flecs::entity zone_e = ecs_world->entity();
                        zone_e.set<plt::CookingZone>({plt::CookingZone_Stove, Rectangle{layer_obj->x, layer_obj->y, layer_obj->width, layer_obj->height}});
                    }
                    else if (std::string("Trash") == layer_obj->name.ptr)
                    {
                        flecs::entity zone_e = ecs_world->entity();
                        zone_e.set<plt::CookingZone>({plt::CookingZone_Trash, Rectangle{layer_obj->x, layer_obj->y, layer_obj->width, layer_obj->height}});
                    }
                    else if (std::string("Plating") == layer_obj->name.ptr)
                    {
                        flecs::entity zone_e = ecs_world->entity();
                        zone_e.set<plt::CookingZone>({plt::CookingZone_Plating, Rectangle{layer_obj->x, layer_obj->y, layer_obj->width, layer_obj->height}});
                    }

                    layer_obj = layer_obj->next;
                }
            }
        }

        layer = layer->next;
    }
}

Map::~Map()
{
    UnloadRenderTexture(map_target);

    for (auto &ts_info : tilesets_info)
        UnloadTexture(ts_info.tex);

    cute_tiled_free_map(map);
}

void Map::draw()
{
    DrawTextureRec(map_target.texture, Rectangle{0, 0, (float)map_target.texture.width, (float)-map_target.texture.height}, Vector2{0, 0}, WHITE);
}

void Map::drawFront()
{
    DrawTextureRec(map_target_front.texture, Rectangle{0, 0, (float)map_target.texture.width, (float)-map_target.texture.height}, Vector2{0, 0}, WHITE);
}
