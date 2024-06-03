# This file is used to manage the dependencies of the ANGLE git repo. It is
# used by gclient to determine what version of each dependency to check out, and
# where.

# Avoids the need for a custom root variable.
use_relative_paths = True

gclient_gn_args_file = 'build/config/gclient_args.gni'

git_dependencies = "SYNC"

gclient_gn_args = [
  'checkout_angle_internal',
  'checkout_angle_mesa',
  'checkout_angle_restricted_traces',
  'generate_location_tags',
]

vars = {
  'android_git': 'https://android.googlesource.com',
  'chromium_git': 'https://chromium.googlesource.com',
  'chrome_internal_git': 'https://chrome-internal.googlesource.com',
  'swiftshader_git': 'https://swiftshader.googlesource.com',
  'dawn_git': 'https://dawn.googlesource.com',

  # This variable is overrided in Chromium's DEPS file.
  'build_with_chromium': False,

  # By default, download the fuchsia sdk from the public sdk directory.
  'fuchsia_sdk_cipd_prefix': 'fuchsia/sdk/core/',

  # We don't use location metadata in our test isolates.
  'generate_location_tags': False,

  # Only check out public sources by default. This can be overridden with custom_vars.
  'checkout_angle_internal': False,

  # Pull in Android native toolchain dependencies for Chrome OS too, so we can
  # build ARC++ support libraries.
  'checkout_android_native_support': 'checkout_android or checkout_chromeos',

  # Check out Mesa and libdrm in ANGLE's third_party folder.
  'checkout_angle_mesa': False,

  # Version of Chromium our Chromium-based DEPS are mirrored from.
  'chromium_revision': '190dbcca7b39c45e185ffe7f9e92da628183e27f',
  # We never want to checkout chromium,
  # but need a dummy DEPS entry for the autoroller
  'dummy_checkout_chromium': False,

  # Current revision of VK-GL-CTS (a.k.a dEQP).
  'vk_gl_cts_revision': 'fd5cc7b1e07578e8e807696034ab894b134f4280',

  # Current revision of googletest.
  # Note: this dep cannot be auto-rolled b/c of nesting.
  'googletest_revision': '2d924d7a971e9667d76ad09727fb2402b4f8a1e3',

  # Current revision of Chrome's third_party googletest directory. This
  # repository is mirrored as a separate repository, with separate git hashes
  # that don't match the external googletest repository or Chrome. Mirrored
  # patches will have a different git hash associated with them.
  # To roll, first get the new hash for chromium_googletest_revision from the
  # mirror of third_party/googletest located here:
  # https://chromium.googlesource.com/chromium/src/third_party/googletest/
  # Then get the new hash for googletest_revision from the root Chrome DEPS
  # file: https://source.chromium.org/chromium/chromium/src/+/main:DEPS
  'chromium_googletest_revision': '17bbed2084d3127bd7bcd27283f18d7a5861bea8',

  # Current revision of jsoncpp.
  # Note: this dep cannot be auto-rolled b/c of nesting.
  'jsoncpp_revision': '42e892d96e47b1f6e29844cc705e148ec4856448',

  # Current revision of Chrome's third_party jsoncpp directory. This repository
  # is mirrored as a separate repository, with separate git hashes that
  # don't match the external JsonCpp repository or Chrome. Mirrored patches
  # will have a different git hash associated with them.
  # To roll, first get the new hash for chromium_jsoncpp_revision from the
  # mirror of third_party/jsoncpp located here:
  # https://chromium.googlesource.com/chromium/src/third_party/jsoncpp/
  # Then get the new hash for jsoncpp_revision from the root Chrome DEPS file:
  # https://source.chromium.org/chromium/chromium/src/+/main:DEPS
  'chromium_jsoncpp_revision': 'f62d44704b4da6014aa231cfc116e7fd29617d2a',

  # Current revision of patched-yasm.
  # Note: this dep cannot be auto-rolled b/c of nesting.
  'patched_yasm_revision': '720b70524a4424b15fc57e82263568c8ba0496ad',

  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling catapult
  # and whatever else without interference from each other.
  'catapult_revision': '4ffc88de803f56cc60b8d6ebfbdb462da8dbb456',

  # the commit queue can handle CLs rolling Fuchsia sdk
  # and whatever else without interference from each other.
  'fuchsia_version': 'version:20.20240529.4.1',

  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling luci-go
  # and whatever else without interference from each other.
  'luci_go': 'git_revision:3c691526411b6ba25a7582ad8aff1f5a8848b7be',

  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling android_sdk_build-tools_version
  # and whatever else without interference from each other.
  'android_sdk_build-tools_version': 'YK9Rzw3fDzMHVzatNN6VlyoD_81amLZpN1AbmkdOd6AC',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling android_sdk_emulator_version
  # and whatever else without interference from each other.
  'android_sdk_emulator_version': '9lGp8nTUCRRWGMnI_96HcKfzjnxEJKUcfvfwmA3wXNkC',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling android_sdk_platform-tools_version
  # and whatever else without interference from each other.
  'android_sdk_platform-tools_version': 'HWVsGs2HCKgSVv41FsOcsfJbNcB0UFiNrF6Tc4yRArYC',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling android_sdk_platforms_version
  # and whatever else without interference from each other.
  'android_sdk_platforms_version': 'u-bhWbTME6u-DjypTgr3ZikCyeAeU6txkR9ET6Uudc8C',

  # ninja CIPD package version.
  # https://chrome-infra-packages.appspot.com/p/infra/3pp/tools/ninja
  'ninja_version': 'version:2@1.11.1.chromium.6',

  # Fetch configuration files required for the 'use_remoteexec' gn arg
  'download_remoteexec_cfg': False,
  # RBE instance to use for running remote builds
  'rbe_instance': Str('projects/rbe-chrome-untrusted/instances/default_instance'),
  # RBE project to download rewrapper config files for. Only needed if
  # different from the project used in 'rbe_instance'
  'rewrapper_cfg_project': Str(''),
  # reclient CIPD package
  'reclient_package': 'infra/rbe/client/',
  # reclient CIPD package version
  'reclient_version': 're_client_version:0.143.0.518e369-gomaip',

  # siso CIPD package version.
  'siso_version': 'git_revision:3c4edd7ed1f9902bb0e422b2257a42600edd3ce5',

  # 'magic' text to tell depot_tools that git submodules should be accepted but
  # but parity with DEPS file is expected.
  'SUBMODULE_MIGRATION': 'True',

   # Make Dawn skip its standalone dependencies
  'dawn_standalone': False,
  'dawn_revision': '9a3d29c786c6eb90603345f2dfdf83e511c50ce3',

  # All of the restricted traces (large).
  'checkout_angle_restricted_traces': 'checkout_angle_internal',

  # Individual vars to enable/disable checkout of only specific traces.
  # === ANGLE Restricted Trace Generated Var Start ===
  'checkout_angle_restricted_trace_1945_air_force': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_20_minutes_till_dawn': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_2_3_4_player_mini_games': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_3d_pool_ball': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_afk_arena': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_age_of_origins_z': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_agent_a': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_aliexpress': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_altos_odyssey': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_among_us': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_angry_birds_2_1500': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_angry_birds_2_launch': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_animal_crossing': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_antutu_refinery': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_arena_of_valor': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_arknights': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_asphalt_8': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_asphalt_9': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_asphalt_9_2024': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_avakin_life': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_aztec_ruins': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_aztec_ruins_high': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_badland': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_basemark_gpu': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_batman_telltale': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_battle_of_polytopia': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_beach_buggy_racing': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_black_clover_m': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_black_desert_mobile': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_blade_and_soul_revolution': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_blockman_go': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_botworld_adventure': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_brawl_stars': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_bricks_breaker_quest': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_bridge_constructor_portal': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_bubble_shooter': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_bubble_shooter_and_friends': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_bus_simulator_indonesia': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_call_break_offline_card_game': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_callbreak': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_candy_crush_500': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_candy_crush_soda_saga': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_car_chase': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_car_parking_multiplayer': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_castlevania_sotn': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_catalyst_black': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_clash_of_clans': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_clash_royale': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_cod_mobile': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_coin_master': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_command_and_conquer_rivals': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_cookie_run_kingdom': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_cookie_run_oven_break': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_csr2_drag_racing': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_cut_the_rope': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_darkness_rises': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_dead_by_daylight': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_dead_cells': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_dead_trigger_2': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_diablo_immortal': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_disney_mirrorverse': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_disney_tsum_tsum': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_dota_underlords': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_dr_driving': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_dragon_ball_legends': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_dragon_ball_z_dokkan_battle': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_dragon_mania_legends': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_dragon_raja': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_driver_overhead_2': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_durak_online': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_efootball_pes_2021': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_egypt_1500': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_eight_ball_pool': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_empires_and_puzzles': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_eve_echoes': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_evony_the_kings_return': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_extreme_car_driving_simulator': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_fallout_shelter_online': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_family_island': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_farm_heroes_saga': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_fate_grand_order': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_fifa_mobile': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_final_fantasy': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_final_fantasy_brave_exvius': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_fire_emblem_heroes': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_fishdom': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_five_nights_at_freddys': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_free_fire': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_free_fire_max': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_gacha_club': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_gacha_life': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_gangstar_vegas': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_gardenscapes': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_genshin_impact': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_geometry_dash': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_goddess_of_victory_nikke': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_google_maps': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_grimvalor': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_happy_color': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_harry_potter_hogwarts_mystery': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_hay_day': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_hearthstone': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_higgs_domino_island': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_hill_climb_racing': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_homescapes': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_honkai_star_rail': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_hungry_shark_evolution': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_idle_heroes': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_infinity_ops': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_injustice_2': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_into_the_dead_2': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_jackpot_world': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_jetpack_joyride': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_junes_journey': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_kartrider_rush': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_kentucky_route_zero': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_klondike_adventures': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_last_shelter_survival': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_league_of_legends_wild_rift': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_lego_legacy': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_life_is_strange': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_lilys_garden': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_limbo': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_lineage_m': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_lords_mobile': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_lotsa_slots': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_ludo_king': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_lumino_city': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_magic_rush_heroes': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_magic_tiles_3': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_manhattan_10': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_manhattan_31': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_mario_kart_tour': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_marvel_contest_of_champions': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_marvel_snap': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_marvel_strike_force': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_merge_dragons': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_messenger_lite': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_minecraft': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_minecraft_bedrock': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_minetest': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_mini_block_craft': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_mini_world': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_mobile_legends': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_modern_combat_5': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_monster_hunter_stories': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_monster_strike': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_monument_valley': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_mortal_kombat': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_mu_origin_3': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_my_friend_pedro': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_my_talking_tom2': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_my_talking_tom_friends': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_nba2k20_800': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_new_legend_of_the_condor_heroes': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_ni_no_kuni': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_nier_reincarnation': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_octopath_traveler': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_off_the_road': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_one_piece_treasure_cruise': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_one_punch_man': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_oxenfree': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_piano_kids': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_plague_inc': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_plants_vs_zombies_2': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_plants_vs_zombies_heroes': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_pokemon_go': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_pokemon_masters_ex': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_pokemon_unite': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_poppy_playtime': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_portal_knights': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_professional_baseball_spirits': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_pubg_mobile_battle_royale': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_pubg_mobile_launch': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_pubg_mobile_lite': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_pubg_mobile_skydive': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_puzzles_and_survival': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_ragnarok_m_eternal_love': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_raid_shadow_legends': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_real_commando_secret_mission': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_real_cricket_20': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_real_gangster_crime': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_real_racing3': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_respawnables': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_retro_bowl': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_rise_of_empires': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_rise_of_kingdoms': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_romancing_saga': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_rope_hero_vice_town': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_royal_match': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_rush_royale': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_saint_seiya_awakening': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_sakura_school_simulator': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_scary_teacher_3d': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_scrabble_go': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_shadow_fight_2': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_shadow_fight_3': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_shadowgun_legends': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_sky_force_reloaded': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_slam_dunk_from_tv_animation': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_slay_the_spire': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_slingshot_test1': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_slingshot_test2': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_sniper_3d': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_solar_smash': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_sonic_forces': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_sonic_the_hedgehog': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_special_forces_group_2': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_standoff_2': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_star_trek_fleet_command': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_star_wars_galaxy_of_heroes': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_star_wars_kotor': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_stardew_valley': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_state_of_survival': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_street_fighter_duel': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_street_fighter_iv_ce': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_streets_of_rage_4': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_stumble_guys': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_subway_princess_runner': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_subway_surfers': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_summoners_war': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_super_mario_run': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_supertuxkart': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_survivor_io': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_talking_tom_hero_dash': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_temple_run_2': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_temple_run_300': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_tesla': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_teslagrad': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_tessellation': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_the_gardens_between': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_the_sims_mobile': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_the_witcher_monster_slayer': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_thimbleweed_park': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_tmnt_shredders_revenge': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_toca_life_world': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_toon_blast': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_top_war': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_township': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_trex_200': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_uber': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_vainglory': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_walking_dead_survivors': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_war_planet_online': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_warcraft_rumble': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_wayward_souls': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_whatsapp': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_words_crush': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_words_of_wonders': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_words_with_friends_2': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_wordscapes': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_world_cricket_championship_2': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_world_of_kings': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_world_of_tanks_blitz': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_world_war_doh': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_worms_zone_io': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_zenonia_4': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_zillow': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_zombie_smasher': 'checkout_angle_restricted_traces',
  # === ANGLE Restricted Trace Generated Var End ===

  'checkout_angle_perfetto': 'checkout_angle_restricted_traces',
}

deps = {

  'build': {
    'url': Var('chromium_git') + '/chromium/src/build.git@5b8c05aed00d5b8a2c6dce05a42dac8f435c443a',
    'condition': 'not build_with_chromium',
  },

  'buildtools': {
    'url': Var('chromium_git') + '/chromium/src/buildtools.git@efa920ce144e4dc1c1841e73179cd7e23b9f0d5e',
    'condition': 'not build_with_chromium',
  },

  'third_party/clang-format/script': {
    'url': Var('chromium_git') + '/external/github.com/llvm/llvm-project/clang/tools/clang-format.git@3c0acd2d4e73dd911309d9e970ba09d58bf23a62',
    'condition': 'not build_with_chromium',
  },

  'buildtools/linux64': {
    'packages': [
      {
        'package': 'gn/gn/linux-${{arch}}',
        'version': 'git_revision:d010969ecc312c4c9512d4b4987b2589087df01a',
      }
    ],
    'dep_type': 'cipd',
    'condition': 'not build_with_chromium and host_os == "linux"',
  },

  'buildtools/mac': {
    'packages': [
      {
        'package': 'gn/gn/mac-${{arch}}',
        'version': 'git_revision:d010969ecc312c4c9512d4b4987b2589087df01a',
      }
    ],
    'dep_type': 'cipd',
    'condition': 'not build_with_chromium and host_os == "mac"',
  },

  'buildtools/reclient': {
    'packages': [
      {
        'package': Var('reclient_package') + '${{platform}}',
        'version': Var('reclient_version'),
      }
    ],
    'dep_type': 'cipd',
    'condition': 'not build_with_chromium and not (host_os == "linux" and host_cpu == "arm64")',
  },

  'buildtools/win': {
    'packages': [
      {
        'package': 'gn/gn/windows-amd64',
        'version': 'git_revision:d010969ecc312c4c9512d4b4987b2589087df01a',
      }
    ],
    'dep_type': 'cipd',
    'condition': 'not build_with_chromium and host_os == "win"',
  },

  'testing': {
    'url': '{chromium_git}/chromium/src/testing@4b8b9d2f220275fbef473974e58167ff80820964',
    'condition': 'not build_with_chromium',
  },

  'third_party/abseil-cpp': {
    'url': Var('chromium_git') + '/chromium/src/third_party/abseil-cpp@30d4d2184638dcc328b79723355c65140cf06e4b',
    'condition': 'not build_with_chromium',
  },

  'third_party/android_build_tools': {
    'url': Var('chromium_git') + '/chromium/src/third_party/android_build_tools@c55fe19074f411e9c2ca28666a536e9d28487c41',
    'condition': 'checkout_android and not build_with_chromium',
  },

  'third_party/android_build_tools/aapt2': {
      'packages': [
          {
              'package': 'chromium/third_party/android_build_tools/aapt2',
              'version': 'G1S0vNnfv3f8FD-9mH5RFSUiK-mnSwri_IdiVQKwLP0C',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_build_tools/art': {
      'packages': [
          {
              'package': 'chromium/third_party/android_build_tools/art',
              'version': '87169fbc701d244c311e6aa8843591a7f1710bc0',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_build_tools/bundletool': {
      'packages': [
          {
               'package': 'chromium/third_party/android_build_tools/bundletool',
               'version': 'CaAT7TJbLQC0LVo1i2TXtaMjK4SZBQ33n-s6DcBbZfgC',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_build_tools/lint': {
      'packages': [
          {
               'package': 'chromium/third_party/android_build_tools/lint',
               'version': 'UTwAxkXgodrL9GZEkxLVNaKg7p14YVDh8SrgP4TnOXcC',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_build_tools/manifest_merger': {
      'packages': [
          {
               'package': 'chromium/third_party/android_build_tools/manifest_merger',
               'version': 'NhxjXSeTx7oy0a_3ilG0QjFMO8YItXf67EW20A_stP8C',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps': {
    'url': Var('chromium_git') + '/chromium/src/third_party/android_deps@2888a739e9b8b8514807a5a0d8b26f91a7e4e91d',
    'condition': 'checkout_android and not build_with_chromium',
  },

  'third_party/android_toolchain/ndk': {
      'packages': [
            {
                'package': 'chromium/third_party/android_toolchain/android_toolchain',
                'version': 'wpJvg81kuXdMM66r_l9Doa-pLfR6S26Jd1x40LpwWEoC',
            },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_platform': {
    'url': Var('chromium_git') + '/chromium/src/third_party/android_platform@eeb2d566f963bb66212fdc0d9bbe1dde550b4969',
    'condition': 'checkout_android and not build_with_chromium',
  },

  'third_party/android_sdk': {
    'url': Var('chromium_git') + '/chromium/src/third_party/android_sdk@f80bca53c2398585538469a4be694fa0feaa30a5',
    'condition': 'checkout_android and not build_with_chromium',
  },

  'third_party/android_sdk/public': {
      'packages': [
          {
              'package': 'chromium/third_party/android_sdk/public/build-tools/34.0.0',
              'version': Var('android_sdk_build-tools_version'),
          },
          {
              'package': 'chromium/third_party/android_sdk/public/emulator',
              'version': Var('android_sdk_emulator_version'),
          },
          {
              'package': 'chromium/third_party/android_sdk/public/platform-tools',
              'version': Var('android_sdk_platform-tools_version'),
          },
          {
              'package': 'chromium/third_party/android_sdk/public/platforms/android-34',
              'version': Var('android_sdk_platforms_version'),
          },
          {
              'package': 'chromium/third_party/android_sdk/public/cmdline-tools',
              'version': 'mU9jm4LkManzjSzRquV1UIA7fHBZ2pK7NtbCXxoVnVUC',
          },
      ],
      'condition': 'checkout_android_native_support and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_system_sdk': {
      'packages': [
          {
              'package': 'chromium/third_party/android_system_sdk/public',
              'version': '4QeolYaSKWBtVTgzJU4tHUfzA9OJTDM8YUcD426IctwC',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/astc-encoder/src': {
    'url': Var('chromium_git') + '/external/github.com/ARM-software/astc-encoder@573c475389bf51d16a5c3fc8348092e094e50e8f',
    'condition': 'not build_with_chromium',
  },

  'third_party/bazel': {
      'packages': [
          {
              'package': 'chromium/third_party/bazel',
              'version': 'VjMsf48QUWw8n7XtJP2AuSjIGmbQeYdWdwyxVvIRLmAC',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/catapult': {
    'url': Var('chromium_git') + '/catapult.git' + '@' + Var('catapult_revision'),
    'condition': 'not build_with_chromium',
  },

  # Cherry is a dEQP/VK-GL-CTS management GUI written in Go. We use it for viewing test results.
  'third_party/cherry': {
    'url': Var('android_git') + '/platform/external/cherry@4f8fb08d33ca5ff05a1c638f04c85bbb8d8b52cc',
    'condition': 'not build_with_chromium',
  },

  'third_party/colorama/src': {
    'url': Var('chromium_git') + '/external/colorama.git@3de9f013df4b470069d03d250224062e8cf15c49',
    'condition': 'checkout_android and not build_with_chromium',
  },

  'third_party/clspv/src': {
    'url': Var('chromium_git') + '/external/github.com/google/clspv@a173c052455434a422bcfe5c12ffe44d574fd6e1',
    'condition': 'not build_with_chromium',
  },

  'third_party/cpu_features/src': {
    'url': Var('chromium_git') + '/external/github.com/google/cpu_features.git' + '@' + '936b9ab5515dead115606559502e3864958f7f6e',
    'condition': 'checkout_android and not build_with_chromium',
  },


  'third_party/dawn': {
    'url': Var('dawn_git') + '/dawn.git' + '@' +  Var('dawn_revision'),
    'condition': 'not build_with_chromium'
  },

  'third_party/depot_tools': {
    'url': Var('chromium_git') + '/chromium/tools/depot_tools.git@6427b94bc2273c9e8a6ab04ef452c5e6c59137bd',
    'condition': 'not build_with_chromium',
  },

  # We never want to checkout chromium,
  # but need a dummy DEPS entry for the autoroller
  'third_party/dummy_chromium': {
    'url': Var('chromium_git') + '/chromium/src.git' + '@' + Var('chromium_revision'),
    'condition': 'dummy_checkout_chromium',
  },

  'third_party/EGL-Registry/src': {
    'url': Var('chromium_git') + '/external/github.com/KhronosGroup/EGL-Registry@7dea2ed79187cd13f76183c4b9100159b9e3e071',
    'condition': 'not build_with_chromium',
  },

  'third_party/flatbuffers/src': {
    'url': Var('chromium_git') + '/external/github.com/google/flatbuffers.git@150644d7f4d030a0629c564fd90dc3becab77636',
    'condition': 'checkout_android and not build_with_chromium',
  },

  'third_party/fuchsia-sdk/sdk': {
      'packages': [
          {
              'package': Var('fuchsia_sdk_cipd_prefix') + '${{platform}}',
              'version': Var('fuchsia_version'),
          },
      ],
      'condition': 'checkout_fuchsia and not build_with_chromium',
      'dep_type': 'cipd',
  },

  # Closed-source OpenGL ES 1.1 Conformance tests.
  'third_party/gles1_conform': {
    'url': Var('chrome_internal_git') + '/angle/es-cts.git@dc9f502f709c9cd88d7f8d3974f1c77aa246958e',
    'condition': 'checkout_angle_internal',
  },

  # glmark2 is a GPL3-licensed OpenGL ES 2.0 benchmark. We use it for testing.
  'third_party/glmark2/src': {
    'url': Var('chromium_git') + '/external/github.com/glmark2/glmark2@ca8de51fedb70bace5351c6b002eb952c747e889',
  },

  'third_party/googletest': {
    'url': Var('chromium_git') + '/chromium/src/third_party/googletest' + '@' + Var('chromium_googletest_revision'),
    'condition': 'not build_with_chromium',
  },

  'third_party/ijar': {
    'url': Var('chromium_git') + '/chromium/src/third_party/ijar@ebabc6deb703af7bff1818706ae7de64980cbd55',
    'condition': 'checkout_android and not build_with_chromium',
  },

  'third_party/kotlin_stdlib': {
      'packages': [
          {
              'package': 'chromium/third_party/kotlin_stdlib',
              'version': '_4e0lDaCjMgaNeq2v2olJs_15Ax3PxGfCU9fMt0FTKcC',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  # We use the upstream/main branch.
  'third_party/libdrm': {
    'url': Var('chromium_git') + '/chromiumos/third_party/libdrm@474894ed17a037a464e5bd845a0765a50f647898',
    'condition': 'checkout_angle_mesa or not build_with_chromium',
  },

  # libjpeg_turbo is used by glmark2.
  'third_party/libjpeg_turbo': {
    'url': Var('chromium_git') + '/chromium/deps/libjpeg_turbo.git@ccfbe1c82a3b6dbe8647ceb36a3f9ee711fba3cf',
    'condition': 'not build_with_chromium',
  },

  'third_party/libpng/src': {
    'url': Var('android_git') + '/platform/external/libpng@d2ece84bd73af1cd5fae5e7574f79b40e5de4fba',
    'condition': 'not build_with_chromium',
  },

  'third_party/llvm/src': {
    'url': Var('chromium_git') + '/external/github.com/llvm/llvm-project@d222fa4521531cc4ac14b8e157d231c108c003be',
    'condition': 'not build_with_chromium',
  },

  'third_party/jdk': {
      'packages': [
          {
              'package': 'chromium/third_party/jdk',
              'version': 'tUJrCBvDNDE9jFvgkuOwX8tU6oCWT8CtI2_JxpGlTJIC',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/jdk/extras': {
      'packages': [
          {
              'package': 'chromium/third_party/jdk/extras',
              'version': '-7m_pvgICYN60yQI3qmTj_8iKjtnT4NXicT0G_jJPqsC',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/jinja2': {
    'url': Var('chromium_git') + '/chromium/src/third_party/jinja2@c9c77525ea20c871a1d4658f8d312b51266d4bad',
    'condition': 'not build_with_chromium',
  },

  'third_party/jsoncpp': {
    'url': Var('chromium_git') + '/chromium/src/third_party/jsoncpp' + '@' + Var('chromium_jsoncpp_revision'),
    'condition': 'not build_with_chromium',
   },

  'third_party/libc++/src': {
    'url': Var('chromium_git') + '/external/github.com/llvm/llvm-project/libcxx.git@852bc6746f45add53fec19f3a29280e69e358d44',
    'condition': 'not build_with_chromium',
  },

  'third_party/libc++abi/src': {
    'url': Var('chromium_git') + '/external/github.com/llvm/llvm-project/libcxxabi.git@5f2c9767ceaeb8a21e4004890bf2c6b08faa8687',
    'condition': 'not build_with_chromium',
  },

  'third_party/libunwind/src': {
    'url': Var('chromium_git') + '/external/github.com/llvm/llvm-project/libunwind.git@a16d528d2574381332f73f385f7590c3aa60f74b',
    'condition': 'not build_with_chromium',
  },

  'third_party/markupsafe': {
    'url': Var('chromium_git') + '/chromium/src/third_party/markupsafe@e582d7f0edb9d67499b0f5abd6ae5550e91da7f2',
    'condition': 'not build_with_chromium',
  },

  # We use the upstream/main branch.
  'third_party/mesa/src': {
    'url': Var('chromium_git') + '/external/github.com/Mesa3D/mesa@0a6aa58acae2a5b27ef783c22e976ec9b0d33ddc',
    'condition': 'checkout_angle_mesa',
  },

  # We use the upstream/master branch.
  'third_party/meson': {
    'url': Var('chromium_git') + '/external/github.com/mesonbuild/meson@9fd5eb605674067ce6f8876dc27e5e116024e8a6',
    'condition': 'checkout_angle_mesa',
  },

  'third_party/nasm': {
    'url': Var('chromium_git') + '/chromium/deps/nasm.git@f477acb1049f5e043904b87b825c5915084a9a29',
    'condition': 'not build_with_chromium',
  },

  'third_party/ninja': {
    'packages': [
      {
        'package': 'infra/3pp/tools/ninja/${{platform}}',
        'version': Var('ninja_version'),
      }
    ],
    'dep_type': 'cipd',
    'condition': 'not build_with_chromium',
  },

  'third_party/OpenCL-CTS/src': {
    'url': Var('chromium_git') + '/external/github.com/KhronosGroup/OpenCL-CTS@e0a31a03fc8f816d59fd8b3051ac6a61d3fa50c6',
    'condition': 'not build_with_chromium',
  },

  'third_party/OpenCL-Docs/src': {
    'url': Var('chromium_git') + '/external/github.com/KhronosGroup/OpenCL-Docs@774114e8761920b976d538d47fad8178d05984ec',
    'condition': 'not build_with_chromium',
  },

  'third_party/OpenCL-ICD-Loader/src': {
    'url': Var('chromium_git') + '/external/github.com/KhronosGroup/OpenCL-ICD-Loader@9b5e3849b49a1448996c8b96ba086cd774d987db',
    'condition': 'not build_with_chromium',
  },

  'third_party/OpenGL-Registry/src': {
    'url': Var('chromium_git') + '/external/github.com/KhronosGroup/OpenGL-Registry@5bae8738b23d06968e7c3a41308568120943ae77',
    'condition': 'not build_with_chromium',
  },

  'third_party/perfetto': {
    'url': Var('android_git') + '/platform/external/perfetto.git@d06bef7807a8b90de9bce77132e188f68459a714',
    'condition': 'not build_with_chromium and checkout_angle_perfetto',
  },

  'third_party/proguard': {
      'packages': [
          {
              'package': 'chromium/third_party/proguard',
              'version': 'Fd91BJFVlmiO6c46YMTsdy7n2f5Sk2hVVGlzPLvqZPsC',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/protobuf': {
    'url': Var('chromium_git') + '/chromium/src/third_party/protobuf@ed9284c473211491ae6de41d60ac0329a79270d8',
    'condition': 'not build_with_chromium',
  },

  'third_party/Python-Markdown': {
    'url': Var('chromium_git') + '/chromium/src/third_party/Python-Markdown@0f4473546172a64636f5d841410c564c0edad625',
    'condition': 'not build_with_chromium',
  },

  'third_party/r8': {
      'packages': [
          {
              'package': 'chromium/third_party/r8',
              'version': 'NCxNylYCpeF52DT5ju1xvvVnuEh3CFBKweSUhjn-IjcC',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  # This duplication is intentional, so we avoid updating the r8.jar used by
  # dexing unless necessary, since each update invalidates all incremental
  # dexing and unnecessarily slows down all bots.
  'third_party/r8/d8': {
      'packages': [
          {
              'package': 'chromium/third_party/r8',
              'version': 'vw5kLlW3-suSlCKSO9OQpFWpR8oDnvQ8k1RgKNUapQYC',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/rapidjson/src': {
    'url': Var('chromium_git') + '/external/github.com/Tencent/rapidjson@781a4e667d84aeedbeb8184b7b62425ea66ec59f',
  },

  'third_party/requests/src': {
    'url': Var('chromium_git') + '/external/github.com/kennethreitz/requests.git@c7e0fc087ceeadb8b4c84a0953a422c474093d6d',
    'condition': 'checkout_android and not build_with_chromium',
  },

  'third_party/siso': {
    'packages': [
      {
        'package': 'infra/build/siso/${{platform}}',
        'version': Var('siso_version'),
      }
    ],
    'dep_type': 'cipd',
    'condition': 'not build_with_chromium',
  },

  'third_party/six': {
    'url': Var('chromium_git') + '/chromium/src/third_party/six@32c68ae5c1fa363e3e86d56a59d230c445f018ac',
    'condition': 'checkout_android and not build_with_chromium',
  },

  'third_party/SwiftShader': {
    'url': Var('swiftshader_git') + '/SwiftShader@4bb94d6c235c255214591665cf94bc840f970f29',
    'condition': 'not build_with_chromium',
  },

  'third_party/turbine': {
      'packages': [
          {
              'package': 'chromium/third_party/turbine',
              'version': 'JA8o86DtHkYnsW4v8F9pdcvi7uqN1WB-L1XFLggZdtAC',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/VK-GL-CTS/src': {
    'url': Var('chromium_git') + '/external/github.com/KhronosGroup/VK-GL-CTS' + '@' + Var('vk_gl_cts_revision'),
  },

  'third_party/vulkan-deps': {
    'url': Var('chromium_git') + '/vulkan-deps@f7e762742da9591045a791977dce67fa577ca017',
    'condition': 'not build_with_chromium',
  },

  'third_party/glslang/src': {
    'url': '{chromium_git}/external/github.com/KhronosGroup/glslang@2b19bf7e1bc0b60cf2fe9d33e5ba6b37dfc1cc83',
    'condition': 'not build_with_chromium',
  },

  'third_party/spirv-cross/src': {
    'url': '{chromium_git}/external/github.com/KhronosGroup/SPIRV-Cross@b8fcf307f1f347089e3c46eb4451d27f32ebc8d3',
    'condition': 'not build_with_chromium',
  },

  'third_party/spirv-headers/src': {
    'url': '{chromium_git}/external/github.com/KhronosGroup/SPIRV-Headers@49a1fceb9b1d087f3c25ad5ec077bb0e46231297',
    'condition': 'not build_with_chromium',
  },

  'third_party/spirv-tools/src': {
    'url': '{chromium_git}/external/github.com/KhronosGroup/SPIRV-Tools@77c40bece1b8b441eee432fc9d74efbf985f777f',
    'condition': 'not build_with_chromium',
  },

  'third_party/vulkan-headers/src': {
    'url': '{chromium_git}/external/github.com/KhronosGroup/Vulkan-Headers@5677bafb820e476441e9e1f745371b72133407d3',
    'condition': 'not build_with_chromium',
  },

  'third_party/vulkan-loader/src': {
    'url': '{chromium_git}/external/github.com/KhronosGroup/Vulkan-Loader@21effda93a239cc4d726b37738c27fdc3e20ec3e',
    'condition': 'not build_with_chromium',
  },

  'third_party/vulkan-tools/src': {
    'url': '{chromium_git}/external/github.com/KhronosGroup/Vulkan-Tools@d67a9d3a394e11c1c4c0f480124f5b7925cb1b4d',
    'condition': 'not build_with_chromium',
  },

  'third_party/vulkan-utility-libraries/src': {
    'url': '{chromium_git}/external/github.com/KhronosGroup/Vulkan-Utility-Libraries@6bc92daa125da7aed037b46513845ebc67bc96e6',
    'condition': 'not build_with_chromium',
  },

  'third_party/vulkan-validation-layers/src': {
    'url': '{chromium_git}/external/github.com/KhronosGroup/Vulkan-ValidationLayers@ff56cf67d3494eec1243cc4225d1667e9b3f90cd',
    'condition': 'not build_with_chromium',
  },

  'third_party/vulkan_memory_allocator': {
    'url': Var('chromium_git') + '/external/github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator@56300b29fbfcc693ee6609ddad3fdd5b7a449a21',
    'condition': 'not build_with_chromium',
  },

  'third_party/wayland': {
    'url': Var('chromium_git') + '/external/anongit.freedesktop.org/git/wayland/wayland@75c1a93e2067220fa06208f20f8f096bb463ec08',
    'condition': 'not build_with_chromium and host_os == "linux"'
  },

  'third_party/zlib': {
    'url': Var('chromium_git') + '/chromium/src/third_party/zlib@209717dd69cd62f24cbacc4758261ae2dd78cfac',
    'condition': 'not build_with_chromium',
  },

  'tools/android': {
    'url': Var('chromium_git') + '/chromium/src/tools/android@34192533e15dd37bd3c91910247fb925d1e15327',
    'condition': 'checkout_android and not build_with_chromium',
  },

  'tools/clang': {
    'url': Var('chromium_git') + '/chromium/src/tools/clang.git@557b28f80de8c10bd00396a3b29ad9edeba5d1dd',
    'condition': 'not build_with_chromium',
  },

  'tools/clang/dsymutil': {
    'packages': [
      {
        'package': 'chromium/llvm-build-tools/dsymutil',
        'version': 'M56jPzDv1620Rnm__jTMYS62Zi8rxHVq7yw0qeBFEgkC',
      }
    ],
    'condition': 'checkout_mac and not build_with_chromium',
    'dep_type': 'cipd',
  },

  'tools/luci-go': {
    'packages': [
      {
        'package': 'infra/tools/luci/isolate/${{platform}}',
        'version': Var('luci_go'),
      },
      {
        'package': 'infra/tools/luci/swarming/${{platform}}',
        'version': Var('luci_go'),
      },
    ],
    'condition': 'not build_with_chromium',
    'dep_type': 'cipd',
  },

  'tools/mb': {
    'url': Var('chromium_git') + '/chromium/src/tools/mb@cececb2a6b96ccd0c677cfd6d0a6e20029a722e0',
    'condition': 'not build_with_chromium',
  },

  'tools/md_browser': {
    'url': Var('chromium_git') + '/chromium/src/tools/md_browser@6cc8e58a83412dc31de6fb7614fadb0b51748d4b',
    'condition': 'not build_with_chromium',
  },

  'tools/memory': {
    'url': Var('chromium_git') + '/chromium/src/tools/memory@c5b623b3ba22e626ff51ab4f0b8d2bde749e970f',
    'condition': 'not build_with_chromium',
  },

  'tools/perf': {
    'url': Var('chromium_git') + '/chromium/src/tools/perf@a900142d9148aa24029be50b9e1426d8c3c8ef4b',
    'condition': 'not build_with_chromium',
  },

  'tools/protoc_wrapper': {
    'url': Var('chromium_git') + '/chromium/src/tools/protoc_wrapper@dbcbea90c20ae1ece442d8ef64e61c7b10e2b013',
    'condition': 'not build_with_chromium',
  },

  'tools/python': {
    'url': Var('chromium_git') + '/chromium/src/tools/python@64dd0e593f8e438764ced983a9f3f96061df748c',
    'condition': 'checkout_android and not build_with_chromium',
  },

  'tools/skia_goldctl/linux': {
      'packages': [
        {
          'package': 'skia/tools/goldctl/linux-amd64',
          'version': 'DwebaDF4oPSZMck-atrwghRPzS584hU4SE0VMntP4sEC',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_linux and not build_with_chromium',
  },

  'tools/skia_goldctl/win': {
      'packages': [
        {
          'package': 'skia/tools/goldctl/windows-amd64',
          'version': '-t8nUSs-QQ1RnwaD9oWH-_3W-avC2ugb8y29Og6Sq1YC',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_win and not build_with_chromium',
  },

  'tools/skia_goldctl/mac_amd64': {
      'packages': [
        {
          'package': 'skia/tools/goldctl/mac-amd64',
          'version': 'K1iF08dyALEazf18lHRmK7k-tvRErDRGAVS3JHxHcbsC',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_mac and not build_with_chromium',
  },

  'tools/skia_goldctl/mac_arm64': {
      'packages': [
        {
          'package': 'skia/tools/goldctl/mac-arm64',
          'version': 'ms8v9LYso2x5_PGXcrV6Jf6ZW_9JYX0yFyJnqB0VrkMC',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_mac and not build_with_chromium',
  },

  'tools/valgrind': {
    'url': Var('chromium_git') + '/chromium/src/tools/valgrind@e10259da244f75e52a681371f679d9ec095ff62a',
    'condition': 'not build_with_chromium',
  },

  # === ANGLE Restricted Trace Generated Code Start ===
  'src/tests/restricted_traces/1945_air_force': {
      'packages': [
        {
            'package': 'angle/traces/1945_air_force',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_1945_air_force',
  },
  'src/tests/restricted_traces/20_minutes_till_dawn': {
      'packages': [
        {
            'package': 'angle/traces/20_minutes_till_dawn',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_20_minutes_till_dawn',
  },
  'src/tests/restricted_traces/2_3_4_player_mini_games': {
      'packages': [
        {
            'package': 'angle/traces/2_3_4_player_mini_games',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_2_3_4_player_mini_games',
  },
  'src/tests/restricted_traces/3d_pool_ball': {
      'packages': [
        {
            'package': 'angle/traces/3d_pool_ball',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_3d_pool_ball',
  },
  'src/tests/restricted_traces/afk_arena': {
      'packages': [
        {
            'package': 'angle/traces/afk_arena',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_afk_arena',
  },
  'src/tests/restricted_traces/age_of_origins_z': {
      'packages': [
        {
            'package': 'angle/traces/age_of_origins_z',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_age_of_origins_z',
  },
  'src/tests/restricted_traces/agent_a': {
      'packages': [
        {
            'package': 'angle/traces/agent_a',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_agent_a',
  },
  'src/tests/restricted_traces/aliexpress': {
      'packages': [
        {
            'package': 'angle/traces/aliexpress',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_aliexpress',
  },
  'src/tests/restricted_traces/altos_odyssey': {
      'packages': [
        {
            'package': 'angle/traces/altos_odyssey',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_altos_odyssey',
  },
  'src/tests/restricted_traces/among_us': {
      'packages': [
        {
            'package': 'angle/traces/among_us',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_among_us',
  },
  'src/tests/restricted_traces/angry_birds_2_1500': {
      'packages': [
        {
            'package': 'angle/traces/angry_birds_2_1500',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_angry_birds_2_1500',
  },
  'src/tests/restricted_traces/angry_birds_2_launch': {
      'packages': [
        {
            'package': 'angle/traces/angry_birds_2_launch',
            'version': 'version:7',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_angry_birds_2_launch',
  },
  'src/tests/restricted_traces/animal_crossing': {
      'packages': [
        {
            'package': 'angle/traces/animal_crossing',
            'version': 'version:4',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_animal_crossing',
  },
  'src/tests/restricted_traces/antutu_refinery': {
      'packages': [
        {
            'package': 'angle/traces/antutu_refinery',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_antutu_refinery',
  },
  'src/tests/restricted_traces/arena_of_valor': {
      'packages': [
        {
            'package': 'angle/traces/arena_of_valor',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_arena_of_valor',
  },
  'src/tests/restricted_traces/arknights': {
      'packages': [
        {
            'package': 'angle/traces/arknights',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_arknights',
  },
  'src/tests/restricted_traces/asphalt_8': {
      'packages': [
        {
            'package': 'angle/traces/asphalt_8',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_asphalt_8',
  },
  'src/tests/restricted_traces/asphalt_9': {
      'packages': [
        {
            'package': 'angle/traces/asphalt_9',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_asphalt_9',
  },
  'src/tests/restricted_traces/asphalt_9_2024': {
      'packages': [
        {
            'package': 'angle/traces/asphalt_9_2024',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_asphalt_9_2024',
  },
  'src/tests/restricted_traces/avakin_life': {
      'packages': [
        {
            'package': 'angle/traces/avakin_life',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_avakin_life',
  },
  'src/tests/restricted_traces/aztec_ruins': {
      'packages': [
        {
            'package': 'angle/traces/aztec_ruins',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_aztec_ruins',
  },
  'src/tests/restricted_traces/aztec_ruins_high': {
      'packages': [
        {
            'package': 'angle/traces/aztec_ruins_high',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_aztec_ruins_high',
  },
  'src/tests/restricted_traces/badland': {
      'packages': [
        {
            'package': 'angle/traces/badland',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_badland',
  },
  'src/tests/restricted_traces/basemark_gpu': {
      'packages': [
        {
            'package': 'angle/traces/basemark_gpu',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_basemark_gpu',
  },
  'src/tests/restricted_traces/batman_telltale': {
      'packages': [
        {
            'package': 'angle/traces/batman_telltale',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_batman_telltale',
  },
  'src/tests/restricted_traces/battle_of_polytopia': {
      'packages': [
        {
            'package': 'angle/traces/battle_of_polytopia',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_battle_of_polytopia',
  },
  'src/tests/restricted_traces/beach_buggy_racing': {
      'packages': [
        {
            'package': 'angle/traces/beach_buggy_racing',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_beach_buggy_racing',
  },
  'src/tests/restricted_traces/black_clover_m': {
      'packages': [
        {
            'package': 'angle/traces/black_clover_m',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_black_clover_m',
  },
  'src/tests/restricted_traces/black_desert_mobile': {
      'packages': [
        {
            'package': 'angle/traces/black_desert_mobile',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_black_desert_mobile',
  },
  'src/tests/restricted_traces/blade_and_soul_revolution': {
      'packages': [
        {
            'package': 'angle/traces/blade_and_soul_revolution',
            'version': 'version:2',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_blade_and_soul_revolution',
  },
  'src/tests/restricted_traces/blockman_go': {
      'packages': [
        {
            'package': 'angle/traces/blockman_go',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_blockman_go',
  },
  'src/tests/restricted_traces/botworld_adventure': {
      'packages': [
        {
            'package': 'angle/traces/botworld_adventure',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_botworld_adventure',
  },
  'src/tests/restricted_traces/brawl_stars': {
      'packages': [
        {
            'package': 'angle/traces/brawl_stars',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_brawl_stars',
  },
  'src/tests/restricted_traces/bricks_breaker_quest': {
      'packages': [
        {
            'package': 'angle/traces/bricks_breaker_quest',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_bricks_breaker_quest',
  },
  'src/tests/restricted_traces/bridge_constructor_portal': {
      'packages': [
        {
            'package': 'angle/traces/bridge_constructor_portal',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_bridge_constructor_portal',
  },
  'src/tests/restricted_traces/bubble_shooter': {
      'packages': [
        {
            'package': 'angle/traces/bubble_shooter',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_bubble_shooter',
  },
  'src/tests/restricted_traces/bubble_shooter_and_friends': {
      'packages': [
        {
            'package': 'angle/traces/bubble_shooter_and_friends',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_bubble_shooter_and_friends',
  },
  'src/tests/restricted_traces/bus_simulator_indonesia': {
      'packages': [
        {
            'package': 'angle/traces/bus_simulator_indonesia',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_bus_simulator_indonesia',
  },
  'src/tests/restricted_traces/call_break_offline_card_game': {
      'packages': [
        {
            'package': 'angle/traces/call_break_offline_card_game',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_call_break_offline_card_game',
  },
  'src/tests/restricted_traces/callbreak': {
      'packages': [
        {
            'package': 'angle/traces/callbreak',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_callbreak',
  },
  'src/tests/restricted_traces/candy_crush_500': {
      'packages': [
        {
            'package': 'angle/traces/candy_crush_500',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_candy_crush_500',
  },
  'src/tests/restricted_traces/candy_crush_soda_saga': {
      'packages': [
        {
            'package': 'angle/traces/candy_crush_soda_saga',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_candy_crush_soda_saga',
  },
  'src/tests/restricted_traces/car_chase': {
      'packages': [
        {
            'package': 'angle/traces/car_chase',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_car_chase',
  },
  'src/tests/restricted_traces/car_parking_multiplayer': {
      'packages': [
        {
            'package': 'angle/traces/car_parking_multiplayer',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_car_parking_multiplayer',
  },
  'src/tests/restricted_traces/castlevania_sotn': {
      'packages': [
        {
            'package': 'angle/traces/castlevania_sotn',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_castlevania_sotn',
  },
  'src/tests/restricted_traces/catalyst_black': {
      'packages': [
        {
            'package': 'angle/traces/catalyst_black',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_catalyst_black',
  },
  'src/tests/restricted_traces/clash_of_clans': {
      'packages': [
        {
            'package': 'angle/traces/clash_of_clans',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_clash_of_clans',
  },
  'src/tests/restricted_traces/clash_royale': {
      'packages': [
        {
            'package': 'angle/traces/clash_royale',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_clash_royale',
  },
  'src/tests/restricted_traces/cod_mobile': {
      'packages': [
        {
            'package': 'angle/traces/cod_mobile',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_cod_mobile',
  },
  'src/tests/restricted_traces/coin_master': {
      'packages': [
        {
            'package': 'angle/traces/coin_master',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_coin_master',
  },
  'src/tests/restricted_traces/command_and_conquer_rivals': {
      'packages': [
        {
            'package': 'angle/traces/command_and_conquer_rivals',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_command_and_conquer_rivals',
  },
  'src/tests/restricted_traces/cookie_run_kingdom': {
      'packages': [
        {
            'package': 'angle/traces/cookie_run_kingdom',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_cookie_run_kingdom',
  },
  'src/tests/restricted_traces/cookie_run_oven_break': {
      'packages': [
        {
            'package': 'angle/traces/cookie_run_oven_break',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_cookie_run_oven_break',
  },
  'src/tests/restricted_traces/csr2_drag_racing': {
      'packages': [
        {
            'package': 'angle/traces/csr2_drag_racing',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_csr2_drag_racing',
  },
  'src/tests/restricted_traces/cut_the_rope': {
      'packages': [
        {
            'package': 'angle/traces/cut_the_rope',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_cut_the_rope',
  },
  'src/tests/restricted_traces/darkness_rises': {
      'packages': [
        {
            'package': 'angle/traces/darkness_rises',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_darkness_rises',
  },
  'src/tests/restricted_traces/dead_by_daylight': {
      'packages': [
        {
            'package': 'angle/traces/dead_by_daylight',
            'version': 'version:2',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_dead_by_daylight',
  },
  'src/tests/restricted_traces/dead_cells': {
      'packages': [
        {
            'package': 'angle/traces/dead_cells',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_dead_cells',
  },
  'src/tests/restricted_traces/dead_trigger_2': {
      'packages': [
        {
            'package': 'angle/traces/dead_trigger_2',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_dead_trigger_2',
  },
  'src/tests/restricted_traces/diablo_immortal': {
      'packages': [
        {
            'package': 'angle/traces/diablo_immortal',
            'version': 'version:2',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_diablo_immortal',
  },
  'src/tests/restricted_traces/disney_mirrorverse': {
      'packages': [
        {
            'package': 'angle/traces/disney_mirrorverse',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_disney_mirrorverse',
  },
  'src/tests/restricted_traces/disney_tsum_tsum': {
      'packages': [
        {
            'package': 'angle/traces/disney_tsum_tsum',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_disney_tsum_tsum',
  },
  'src/tests/restricted_traces/dota_underlords': {
      'packages': [
        {
            'package': 'angle/traces/dota_underlords',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_dota_underlords',
  },
  'src/tests/restricted_traces/dr_driving': {
      'packages': [
        {
            'package': 'angle/traces/dr_driving',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_dr_driving',
  },
  'src/tests/restricted_traces/dragon_ball_legends': {
      'packages': [
        {
            'package': 'angle/traces/dragon_ball_legends',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_dragon_ball_legends',
  },
  'src/tests/restricted_traces/dragon_ball_z_dokkan_battle': {
      'packages': [
        {
            'package': 'angle/traces/dragon_ball_z_dokkan_battle',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_dragon_ball_z_dokkan_battle',
  },
  'src/tests/restricted_traces/dragon_mania_legends': {
      'packages': [
        {
            'package': 'angle/traces/dragon_mania_legends',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_dragon_mania_legends',
  },
  'src/tests/restricted_traces/dragon_raja': {
      'packages': [
        {
            'package': 'angle/traces/dragon_raja',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_dragon_raja',
  },
  'src/tests/restricted_traces/driver_overhead_2': {
      'packages': [
        {
            'package': 'angle/traces/driver_overhead_2',
            'version': 'version:2',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_driver_overhead_2',
  },
  'src/tests/restricted_traces/durak_online': {
      'packages': [
        {
            'package': 'angle/traces/durak_online',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_durak_online',
  },
  'src/tests/restricted_traces/efootball_pes_2021': {
      'packages': [
        {
            'package': 'angle/traces/efootball_pes_2021',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_efootball_pes_2021',
  },
  'src/tests/restricted_traces/egypt_1500': {
      'packages': [
        {
            'package': 'angle/traces/egypt_1500',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_egypt_1500',
  },
  'src/tests/restricted_traces/eight_ball_pool': {
      'packages': [
        {
            'package': 'angle/traces/eight_ball_pool',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_eight_ball_pool',
  },
  'src/tests/restricted_traces/empires_and_puzzles': {
      'packages': [
        {
            'package': 'angle/traces/empires_and_puzzles',
            'version': 'version:2',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_empires_and_puzzles',
  },
  'src/tests/restricted_traces/eve_echoes': {
      'packages': [
        {
            'package': 'angle/traces/eve_echoes',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_eve_echoes',
  },
  'src/tests/restricted_traces/evony_the_kings_return': {
      'packages': [
        {
            'package': 'angle/traces/evony_the_kings_return',
            'version': 'version:2',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_evony_the_kings_return',
  },
  'src/tests/restricted_traces/extreme_car_driving_simulator': {
      'packages': [
        {
            'package': 'angle/traces/extreme_car_driving_simulator',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_extreme_car_driving_simulator',
  },
  'src/tests/restricted_traces/fallout_shelter_online': {
      'packages': [
        {
            'package': 'angle/traces/fallout_shelter_online',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_fallout_shelter_online',
  },
  'src/tests/restricted_traces/family_island': {
      'packages': [
        {
            'package': 'angle/traces/family_island',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_family_island',
  },
  'src/tests/restricted_traces/farm_heroes_saga': {
      'packages': [
        {
            'package': 'angle/traces/farm_heroes_saga',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_farm_heroes_saga',
  },
  'src/tests/restricted_traces/fate_grand_order': {
      'packages': [
        {
            'package': 'angle/traces/fate_grand_order',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_fate_grand_order',
  },
  'src/tests/restricted_traces/fifa_mobile': {
      'packages': [
        {
            'package': 'angle/traces/fifa_mobile',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_fifa_mobile',
  },
  'src/tests/restricted_traces/final_fantasy': {
      'packages': [
        {
            'package': 'angle/traces/final_fantasy',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_final_fantasy',
  },
  'src/tests/restricted_traces/final_fantasy_brave_exvius': {
      'packages': [
        {
            'package': 'angle/traces/final_fantasy_brave_exvius',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_final_fantasy_brave_exvius',
  },
  'src/tests/restricted_traces/fire_emblem_heroes': {
      'packages': [
        {
            'package': 'angle/traces/fire_emblem_heroes',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_fire_emblem_heroes',
  },
  'src/tests/restricted_traces/fishdom': {
      'packages': [
        {
            'package': 'angle/traces/fishdom',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_fishdom',
  },
  'src/tests/restricted_traces/five_nights_at_freddys': {
      'packages': [
        {
            'package': 'angle/traces/five_nights_at_freddys',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_five_nights_at_freddys',
  },
  'src/tests/restricted_traces/free_fire': {
      'packages': [
        {
            'package': 'angle/traces/free_fire',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_free_fire',
  },
  'src/tests/restricted_traces/free_fire_max': {
      'packages': [
        {
            'package': 'angle/traces/free_fire_max',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_free_fire_max',
  },
  'src/tests/restricted_traces/gacha_club': {
      'packages': [
        {
            'package': 'angle/traces/gacha_club',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_gacha_club',
  },
  'src/tests/restricted_traces/gacha_life': {
      'packages': [
        {
            'package': 'angle/traces/gacha_life',
            'version': 'version:2',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_gacha_life',
  },
  'src/tests/restricted_traces/gangstar_vegas': {
      'packages': [
        {
            'package': 'angle/traces/gangstar_vegas',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_gangstar_vegas',
  },
  'src/tests/restricted_traces/gardenscapes': {
      'packages': [
        {
            'package': 'angle/traces/gardenscapes',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_gardenscapes',
  },
  'src/tests/restricted_traces/genshin_impact': {
      'packages': [
        {
            'package': 'angle/traces/genshin_impact',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_genshin_impact',
  },
  'src/tests/restricted_traces/geometry_dash': {
      'packages': [
        {
            'package': 'angle/traces/geometry_dash',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_geometry_dash',
  },
  'src/tests/restricted_traces/goddess_of_victory_nikke': {
      'packages': [
        {
            'package': 'angle/traces/goddess_of_victory_nikke',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_goddess_of_victory_nikke',
  },
  'src/tests/restricted_traces/google_maps': {
      'packages': [
        {
            'package': 'angle/traces/google_maps',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_google_maps',
  },
  'src/tests/restricted_traces/grimvalor': {
      'packages': [
        {
            'package': 'angle/traces/grimvalor',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_grimvalor',
  },
  'src/tests/restricted_traces/happy_color': {
      'packages': [
        {
            'package': 'angle/traces/happy_color',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_happy_color',
  },
  'src/tests/restricted_traces/harry_potter_hogwarts_mystery': {
      'packages': [
        {
            'package': 'angle/traces/harry_potter_hogwarts_mystery',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_harry_potter_hogwarts_mystery',
  },
  'src/tests/restricted_traces/hay_day': {
      'packages': [
        {
            'package': 'angle/traces/hay_day',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_hay_day',
  },
  'src/tests/restricted_traces/hearthstone': {
      'packages': [
        {
            'package': 'angle/traces/hearthstone',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_hearthstone',
  },
  'src/tests/restricted_traces/higgs_domino_island': {
      'packages': [
        {
            'package': 'angle/traces/higgs_domino_island',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_higgs_domino_island',
  },
  'src/tests/restricted_traces/hill_climb_racing': {
      'packages': [
        {
            'package': 'angle/traces/hill_climb_racing',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_hill_climb_racing',
  },
  'src/tests/restricted_traces/homescapes': {
      'packages': [
        {
            'package': 'angle/traces/homescapes',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_homescapes',
  },
  'src/tests/restricted_traces/honkai_star_rail': {
      'packages': [
        {
            'package': 'angle/traces/honkai_star_rail',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_honkai_star_rail',
  },
  'src/tests/restricted_traces/hungry_shark_evolution': {
      'packages': [
        {
            'package': 'angle/traces/hungry_shark_evolution',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_hungry_shark_evolution',
  },
  'src/tests/restricted_traces/idle_heroes': {
      'packages': [
        {
            'package': 'angle/traces/idle_heroes',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_idle_heroes',
  },
  'src/tests/restricted_traces/infinity_ops': {
      'packages': [
        {
            'package': 'angle/traces/infinity_ops',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_infinity_ops',
  },
  'src/tests/restricted_traces/injustice_2': {
      'packages': [
        {
            'package': 'angle/traces/injustice_2',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_injustice_2',
  },
  'src/tests/restricted_traces/into_the_dead_2': {
      'packages': [
        {
            'package': 'angle/traces/into_the_dead_2',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_into_the_dead_2',
  },
  'src/tests/restricted_traces/jackpot_world': {
      'packages': [
        {
            'package': 'angle/traces/jackpot_world',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_jackpot_world',
  },
  'src/tests/restricted_traces/jetpack_joyride': {
      'packages': [
        {
            'package': 'angle/traces/jetpack_joyride',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_jetpack_joyride',
  },
  'src/tests/restricted_traces/junes_journey': {
      'packages': [
        {
            'package': 'angle/traces/junes_journey',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_junes_journey',
  },
  'src/tests/restricted_traces/kartrider_rush': {
      'packages': [
        {
            'package': 'angle/traces/kartrider_rush',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_kartrider_rush',
  },
  'src/tests/restricted_traces/kentucky_route_zero': {
      'packages': [
        {
            'package': 'angle/traces/kentucky_route_zero',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_kentucky_route_zero',
  },
  'src/tests/restricted_traces/klondike_adventures': {
      'packages': [
        {
            'package': 'angle/traces/klondike_adventures',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_klondike_adventures',
  },
  'src/tests/restricted_traces/last_shelter_survival': {
      'packages': [
        {
            'package': 'angle/traces/last_shelter_survival',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_last_shelter_survival',
  },
  'src/tests/restricted_traces/league_of_legends_wild_rift': {
      'packages': [
        {
            'package': 'angle/traces/league_of_legends_wild_rift',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_league_of_legends_wild_rift',
  },
  'src/tests/restricted_traces/lego_legacy': {
      'packages': [
        {
            'package': 'angle/traces/lego_legacy',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_lego_legacy',
  },
  'src/tests/restricted_traces/life_is_strange': {
      'packages': [
        {
            'package': 'angle/traces/life_is_strange',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_life_is_strange',
  },
  'src/tests/restricted_traces/lilys_garden': {
      'packages': [
        {
            'package': 'angle/traces/lilys_garden',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_lilys_garden',
  },
  'src/tests/restricted_traces/limbo': {
      'packages': [
        {
            'package': 'angle/traces/limbo',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_limbo',
  },
  'src/tests/restricted_traces/lineage_m': {
      'packages': [
        {
            'package': 'angle/traces/lineage_m',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_lineage_m',
  },
  'src/tests/restricted_traces/lords_mobile': {
      'packages': [
        {
            'package': 'angle/traces/lords_mobile',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_lords_mobile',
  },
  'src/tests/restricted_traces/lotsa_slots': {
      'packages': [
        {
            'package': 'angle/traces/lotsa_slots',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_lotsa_slots',
  },
  'src/tests/restricted_traces/ludo_king': {
      'packages': [
        {
            'package': 'angle/traces/ludo_king',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_ludo_king',
  },
  'src/tests/restricted_traces/lumino_city': {
      'packages': [
        {
            'package': 'angle/traces/lumino_city',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_lumino_city',
  },
  'src/tests/restricted_traces/magic_rush_heroes': {
      'packages': [
        {
            'package': 'angle/traces/magic_rush_heroes',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_magic_rush_heroes',
  },
  'src/tests/restricted_traces/magic_tiles_3': {
      'packages': [
        {
            'package': 'angle/traces/magic_tiles_3',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_magic_tiles_3',
  },
  'src/tests/restricted_traces/manhattan_10': {
      'packages': [
        {
            'package': 'angle/traces/manhattan_10',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_manhattan_10',
  },
  'src/tests/restricted_traces/manhattan_31': {
      'packages': [
        {
            'package': 'angle/traces/manhattan_31',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_manhattan_31',
  },
  'src/tests/restricted_traces/mario_kart_tour': {
      'packages': [
        {
            'package': 'angle/traces/mario_kart_tour',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_mario_kart_tour',
  },
  'src/tests/restricted_traces/marvel_contest_of_champions': {
      'packages': [
        {
            'package': 'angle/traces/marvel_contest_of_champions',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_marvel_contest_of_champions',
  },
  'src/tests/restricted_traces/marvel_snap': {
      'packages': [
        {
            'package': 'angle/traces/marvel_snap',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_marvel_snap',
  },
  'src/tests/restricted_traces/marvel_strike_force': {
      'packages': [
        {
            'package': 'angle/traces/marvel_strike_force',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_marvel_strike_force',
  },
  'src/tests/restricted_traces/merge_dragons': {
      'packages': [
        {
            'package': 'angle/traces/merge_dragons',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_merge_dragons',
  },
  'src/tests/restricted_traces/messenger_lite': {
      'packages': [
        {
            'package': 'angle/traces/messenger_lite',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_messenger_lite',
  },
  'src/tests/restricted_traces/minecraft': {
      'packages': [
        {
            'package': 'angle/traces/minecraft',
            'version': 'version:7',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_minecraft',
  },
  'src/tests/restricted_traces/minecraft_bedrock': {
      'packages': [
        {
            'package': 'angle/traces/minecraft_bedrock',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_minecraft_bedrock',
  },
  'src/tests/restricted_traces/minetest': {
      'packages': [
        {
            'package': 'angle/traces/minetest',
            'version': 'version:2',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_minetest',
  },
  'src/tests/restricted_traces/mini_block_craft': {
      'packages': [
        {
            'package': 'angle/traces/mini_block_craft',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_mini_block_craft',
  },
  'src/tests/restricted_traces/mini_world': {
      'packages': [
        {
            'package': 'angle/traces/mini_world',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_mini_world',
  },
  'src/tests/restricted_traces/mobile_legends': {
      'packages': [
        {
            'package': 'angle/traces/mobile_legends',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_mobile_legends',
  },
  'src/tests/restricted_traces/modern_combat_5': {
      'packages': [
        {
            'package': 'angle/traces/modern_combat_5',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_modern_combat_5',
  },
  'src/tests/restricted_traces/monster_hunter_stories': {
      'packages': [
        {
            'package': 'angle/traces/monster_hunter_stories',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_monster_hunter_stories',
  },
  'src/tests/restricted_traces/monster_strike': {
      'packages': [
        {
            'package': 'angle/traces/monster_strike',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_monster_strike',
  },
  'src/tests/restricted_traces/monument_valley': {
      'packages': [
        {
            'package': 'angle/traces/monument_valley',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_monument_valley',
  },
  'src/tests/restricted_traces/mortal_kombat': {
      'packages': [
        {
            'package': 'angle/traces/mortal_kombat',
            'version': 'version:2',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_mortal_kombat',
  },
  'src/tests/restricted_traces/mu_origin_3': {
      'packages': [
        {
            'package': 'angle/traces/mu_origin_3',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_mu_origin_3',
  },
  'src/tests/restricted_traces/my_friend_pedro': {
      'packages': [
        {
            'package': 'angle/traces/my_friend_pedro',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_my_friend_pedro',
  },
  'src/tests/restricted_traces/my_talking_tom2': {
      'packages': [
        {
            'package': 'angle/traces/my_talking_tom2',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_my_talking_tom2',
  },
  'src/tests/restricted_traces/my_talking_tom_friends': {
      'packages': [
        {
            'package': 'angle/traces/my_talking_tom_friends',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_my_talking_tom_friends',
  },
  'src/tests/restricted_traces/nba2k20_800': {
      'packages': [
        {
            'package': 'angle/traces/nba2k20_800',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_nba2k20_800',
  },
  'src/tests/restricted_traces/new_legend_of_the_condor_heroes': {
      'packages': [
        {
            'package': 'angle/traces/new_legend_of_the_condor_heroes',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_new_legend_of_the_condor_heroes',
  },
  'src/tests/restricted_traces/ni_no_kuni': {
      'packages': [
        {
            'package': 'angle/traces/ni_no_kuni',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_ni_no_kuni',
  },
  'src/tests/restricted_traces/nier_reincarnation': {
      'packages': [
        {
            'package': 'angle/traces/nier_reincarnation',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_nier_reincarnation',
  },
  'src/tests/restricted_traces/octopath_traveler': {
      'packages': [
        {
            'package': 'angle/traces/octopath_traveler',
            'version': 'version:2',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_octopath_traveler',
  },
  'src/tests/restricted_traces/off_the_road': {
      'packages': [
        {
            'package': 'angle/traces/off_the_road',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_off_the_road',
  },
  'src/tests/restricted_traces/one_piece_treasure_cruise': {
      'packages': [
        {
            'package': 'angle/traces/one_piece_treasure_cruise',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_one_piece_treasure_cruise',
  },
  'src/tests/restricted_traces/one_punch_man': {
      'packages': [
        {
            'package': 'angle/traces/one_punch_man',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_one_punch_man',
  },
  'src/tests/restricted_traces/oxenfree': {
      'packages': [
        {
            'package': 'angle/traces/oxenfree',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_oxenfree',
  },
  'src/tests/restricted_traces/piano_kids': {
      'packages': [
        {
            'package': 'angle/traces/piano_kids',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_piano_kids',
  },
  'src/tests/restricted_traces/plague_inc': {
      'packages': [
        {
            'package': 'angle/traces/plague_inc',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_plague_inc',
  },
  'src/tests/restricted_traces/plants_vs_zombies_2': {
      'packages': [
        {
            'package': 'angle/traces/plants_vs_zombies_2',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_plants_vs_zombies_2',
  },
  'src/tests/restricted_traces/plants_vs_zombies_heroes': {
      'packages': [
        {
            'package': 'angle/traces/plants_vs_zombies_heroes',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_plants_vs_zombies_heroes',
  },
  'src/tests/restricted_traces/pokemon_go': {
      'packages': [
        {
            'package': 'angle/traces/pokemon_go',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_pokemon_go',
  },
  'src/tests/restricted_traces/pokemon_masters_ex': {
      'packages': [
        {
            'package': 'angle/traces/pokemon_masters_ex',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_pokemon_masters_ex',
  },
  'src/tests/restricted_traces/pokemon_unite': {
      'packages': [
        {
            'package': 'angle/traces/pokemon_unite',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_pokemon_unite',
  },
  'src/tests/restricted_traces/poppy_playtime': {
      'packages': [
        {
            'package': 'angle/traces/poppy_playtime',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_poppy_playtime',
  },
  'src/tests/restricted_traces/portal_knights': {
      'packages': [
        {
            'package': 'angle/traces/portal_knights',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_portal_knights',
  },
  'src/tests/restricted_traces/professional_baseball_spirits': {
      'packages': [
        {
            'package': 'angle/traces/professional_baseball_spirits',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_professional_baseball_spirits',
  },
  'src/tests/restricted_traces/pubg_mobile_battle_royale': {
      'packages': [
        {
            'package': 'angle/traces/pubg_mobile_battle_royale',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_pubg_mobile_battle_royale',
  },
  'src/tests/restricted_traces/pubg_mobile_launch': {
      'packages': [
        {
            'package': 'angle/traces/pubg_mobile_launch',
            'version': 'version:6',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_pubg_mobile_launch',
  },
  'src/tests/restricted_traces/pubg_mobile_lite': {
      'packages': [
        {
            'package': 'angle/traces/pubg_mobile_lite',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_pubg_mobile_lite',
  },
  'src/tests/restricted_traces/pubg_mobile_skydive': {
      'packages': [
        {
            'package': 'angle/traces/pubg_mobile_skydive',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_pubg_mobile_skydive',
  },
  'src/tests/restricted_traces/puzzles_and_survival': {
      'packages': [
        {
            'package': 'angle/traces/puzzles_and_survival',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_puzzles_and_survival',
  },
  'src/tests/restricted_traces/ragnarok_m_eternal_love': {
      'packages': [
        {
            'package': 'angle/traces/ragnarok_m_eternal_love',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_ragnarok_m_eternal_love',
  },
  'src/tests/restricted_traces/raid_shadow_legends': {
      'packages': [
        {
            'package': 'angle/traces/raid_shadow_legends',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_raid_shadow_legends',
  },
  'src/tests/restricted_traces/real_commando_secret_mission': {
      'packages': [
        {
            'package': 'angle/traces/real_commando_secret_mission',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_real_commando_secret_mission',
  },
  'src/tests/restricted_traces/real_cricket_20': {
      'packages': [
        {
            'package': 'angle/traces/real_cricket_20',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_real_cricket_20',
  },
  'src/tests/restricted_traces/real_gangster_crime': {
      'packages': [
        {
            'package': 'angle/traces/real_gangster_crime',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_real_gangster_crime',
  },
  'src/tests/restricted_traces/real_racing3': {
      'packages': [
        {
            'package': 'angle/traces/real_racing3',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_real_racing3',
  },
  'src/tests/restricted_traces/respawnables': {
      'packages': [
        {
            'package': 'angle/traces/respawnables',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_respawnables',
  },
  'src/tests/restricted_traces/retro_bowl': {
      'packages': [
        {
            'package': 'angle/traces/retro_bowl',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_retro_bowl',
  },
  'src/tests/restricted_traces/rise_of_empires': {
      'packages': [
        {
            'package': 'angle/traces/rise_of_empires',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_rise_of_empires',
  },
  'src/tests/restricted_traces/rise_of_kingdoms': {
      'packages': [
        {
            'package': 'angle/traces/rise_of_kingdoms',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_rise_of_kingdoms',
  },
  'src/tests/restricted_traces/romancing_saga': {
      'packages': [
        {
            'package': 'angle/traces/romancing_saga',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_romancing_saga',
  },
  'src/tests/restricted_traces/rope_hero_vice_town': {
      'packages': [
        {
            'package': 'angle/traces/rope_hero_vice_town',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_rope_hero_vice_town',
  },
  'src/tests/restricted_traces/royal_match': {
      'packages': [
        {
            'package': 'angle/traces/royal_match',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_royal_match',
  },
  'src/tests/restricted_traces/rush_royale': {
      'packages': [
        {
            'package': 'angle/traces/rush_royale',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_rush_royale',
  },
  'src/tests/restricted_traces/saint_seiya_awakening': {
      'packages': [
        {
            'package': 'angle/traces/saint_seiya_awakening',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_saint_seiya_awakening',
  },
  'src/tests/restricted_traces/sakura_school_simulator': {
      'packages': [
        {
            'package': 'angle/traces/sakura_school_simulator',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_sakura_school_simulator',
  },
  'src/tests/restricted_traces/scary_teacher_3d': {
      'packages': [
        {
            'package': 'angle/traces/scary_teacher_3d',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_scary_teacher_3d',
  },
  'src/tests/restricted_traces/scrabble_go': {
      'packages': [
        {
            'package': 'angle/traces/scrabble_go',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_scrabble_go',
  },
  'src/tests/restricted_traces/shadow_fight_2': {
      'packages': [
        {
            'package': 'angle/traces/shadow_fight_2',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_shadow_fight_2',
  },
  'src/tests/restricted_traces/shadow_fight_3': {
      'packages': [
        {
            'package': 'angle/traces/shadow_fight_3',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_shadow_fight_3',
  },
  'src/tests/restricted_traces/shadowgun_legends': {
      'packages': [
        {
            'package': 'angle/traces/shadowgun_legends',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_shadowgun_legends',
  },
  'src/tests/restricted_traces/sky_force_reloaded': {
      'packages': [
        {
            'package': 'angle/traces/sky_force_reloaded',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_sky_force_reloaded',
  },
  'src/tests/restricted_traces/slam_dunk_from_tv_animation': {
      'packages': [
        {
            'package': 'angle/traces/slam_dunk_from_tv_animation',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_slam_dunk_from_tv_animation',
  },
  'src/tests/restricted_traces/slay_the_spire': {
      'packages': [
        {
            'package': 'angle/traces/slay_the_spire',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_slay_the_spire',
  },
  'src/tests/restricted_traces/slingshot_test1': {
      'packages': [
        {
            'package': 'angle/traces/slingshot_test1',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_slingshot_test1',
  },
  'src/tests/restricted_traces/slingshot_test2': {
      'packages': [
        {
            'package': 'angle/traces/slingshot_test2',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_slingshot_test2',
  },
  'src/tests/restricted_traces/sniper_3d': {
      'packages': [
        {
            'package': 'angle/traces/sniper_3d',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_sniper_3d',
  },
  'src/tests/restricted_traces/solar_smash': {
      'packages': [
        {
            'package': 'angle/traces/solar_smash',
            'version': 'version:2',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_solar_smash',
  },
  'src/tests/restricted_traces/sonic_forces': {
      'packages': [
        {
            'package': 'angle/traces/sonic_forces',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_sonic_forces',
  },
  'src/tests/restricted_traces/sonic_the_hedgehog': {
      'packages': [
        {
            'package': 'angle/traces/sonic_the_hedgehog',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_sonic_the_hedgehog',
  },
  'src/tests/restricted_traces/special_forces_group_2': {
      'packages': [
        {
            'package': 'angle/traces/special_forces_group_2',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_special_forces_group_2',
  },
  'src/tests/restricted_traces/standoff_2': {
      'packages': [
        {
            'package': 'angle/traces/standoff_2',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_standoff_2',
  },
  'src/tests/restricted_traces/star_trek_fleet_command': {
      'packages': [
        {
            'package': 'angle/traces/star_trek_fleet_command',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_star_trek_fleet_command',
  },
  'src/tests/restricted_traces/star_wars_galaxy_of_heroes': {
      'packages': [
        {
            'package': 'angle/traces/star_wars_galaxy_of_heroes',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_star_wars_galaxy_of_heroes',
  },
  'src/tests/restricted_traces/star_wars_kotor': {
      'packages': [
        {
            'package': 'angle/traces/star_wars_kotor',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_star_wars_kotor',
  },
  'src/tests/restricted_traces/stardew_valley': {
      'packages': [
        {
            'package': 'angle/traces/stardew_valley',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_stardew_valley',
  },
  'src/tests/restricted_traces/state_of_survival': {
      'packages': [
        {
            'package': 'angle/traces/state_of_survival',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_state_of_survival',
  },
  'src/tests/restricted_traces/street_fighter_duel': {
      'packages': [
        {
            'package': 'angle/traces/street_fighter_duel',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_street_fighter_duel',
  },
  'src/tests/restricted_traces/street_fighter_iv_ce': {
      'packages': [
        {
            'package': 'angle/traces/street_fighter_iv_ce',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_street_fighter_iv_ce',
  },
  'src/tests/restricted_traces/streets_of_rage_4': {
      'packages': [
        {
            'package': 'angle/traces/streets_of_rage_4',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_streets_of_rage_4',
  },
  'src/tests/restricted_traces/stumble_guys': {
      'packages': [
        {
            'package': 'angle/traces/stumble_guys',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_stumble_guys',
  },
  'src/tests/restricted_traces/subway_princess_runner': {
      'packages': [
        {
            'package': 'angle/traces/subway_princess_runner',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_subway_princess_runner',
  },
  'src/tests/restricted_traces/subway_surfers': {
      'packages': [
        {
            'package': 'angle/traces/subway_surfers',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_subway_surfers',
  },
  'src/tests/restricted_traces/summoners_war': {
      'packages': [
        {
            'package': 'angle/traces/summoners_war',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_summoners_war',
  },
  'src/tests/restricted_traces/super_mario_run': {
      'packages': [
        {
            'package': 'angle/traces/super_mario_run',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_super_mario_run',
  },
  'src/tests/restricted_traces/supertuxkart': {
      'packages': [
        {
            'package': 'angle/traces/supertuxkart',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_supertuxkart',
  },
  'src/tests/restricted_traces/survivor_io': {
      'packages': [
        {
            'package': 'angle/traces/survivor_io',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_survivor_io',
  },
  'src/tests/restricted_traces/talking_tom_hero_dash': {
      'packages': [
        {
            'package': 'angle/traces/talking_tom_hero_dash',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_talking_tom_hero_dash',
  },
  'src/tests/restricted_traces/temple_run_2': {
      'packages': [
        {
            'package': 'angle/traces/temple_run_2',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_temple_run_2',
  },
  'src/tests/restricted_traces/temple_run_300': {
      'packages': [
        {
            'package': 'angle/traces/temple_run_300',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_temple_run_300',
  },
  'src/tests/restricted_traces/tesla': {
      'packages': [
        {
            'package': 'angle/traces/tesla',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_tesla',
  },
  'src/tests/restricted_traces/teslagrad': {
      'packages': [
        {
            'package': 'angle/traces/teslagrad',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_teslagrad',
  },
  'src/tests/restricted_traces/tessellation': {
      'packages': [
        {
            'package': 'angle/traces/tessellation',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_tessellation',
  },
  'src/tests/restricted_traces/the_gardens_between': {
      'packages': [
        {
            'package': 'angle/traces/the_gardens_between',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_the_gardens_between',
  },
  'src/tests/restricted_traces/the_sims_mobile': {
      'packages': [
        {
            'package': 'angle/traces/the_sims_mobile',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_the_sims_mobile',
  },
  'src/tests/restricted_traces/the_witcher_monster_slayer': {
      'packages': [
        {
            'package': 'angle/traces/the_witcher_monster_slayer',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_the_witcher_monster_slayer',
  },
  'src/tests/restricted_traces/thimbleweed_park': {
      'packages': [
        {
            'package': 'angle/traces/thimbleweed_park',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_thimbleweed_park',
  },
  'src/tests/restricted_traces/tmnt_shredders_revenge': {
      'packages': [
        {
            'package': 'angle/traces/tmnt_shredders_revenge',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_tmnt_shredders_revenge',
  },
  'src/tests/restricted_traces/toca_life_world': {
      'packages': [
        {
            'package': 'angle/traces/toca_life_world',
            'version': 'version:2',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_toca_life_world',
  },
  'src/tests/restricted_traces/toon_blast': {
      'packages': [
        {
            'package': 'angle/traces/toon_blast',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_toon_blast',
  },
  'src/tests/restricted_traces/top_war': {
      'packages': [
        {
            'package': 'angle/traces/top_war',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_top_war',
  },
  'src/tests/restricted_traces/township': {
      'packages': [
        {
            'package': 'angle/traces/township',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_township',
  },
  'src/tests/restricted_traces/trex_200': {
      'packages': [
        {
            'package': 'angle/traces/trex_200',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_trex_200',
  },
  'src/tests/restricted_traces/uber': {
      'packages': [
        {
            'package': 'angle/traces/uber',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_uber',
  },
  'src/tests/restricted_traces/vainglory': {
      'packages': [
        {
            'package': 'angle/traces/vainglory',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_vainglory',
  },
  'src/tests/restricted_traces/walking_dead_survivors': {
      'packages': [
        {
            'package': 'angle/traces/walking_dead_survivors',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_walking_dead_survivors',
  },
  'src/tests/restricted_traces/war_planet_online': {
      'packages': [
        {
            'package': 'angle/traces/war_planet_online',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_war_planet_online',
  },
  'src/tests/restricted_traces/warcraft_rumble': {
      'packages': [
        {
            'package': 'angle/traces/warcraft_rumble',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_warcraft_rumble',
  },
  'src/tests/restricted_traces/wayward_souls': {
      'packages': [
        {
            'package': 'angle/traces/wayward_souls',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_wayward_souls',
  },
  'src/tests/restricted_traces/whatsapp': {
      'packages': [
        {
            'package': 'angle/traces/whatsapp',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_whatsapp',
  },
  'src/tests/restricted_traces/words_crush': {
      'packages': [
        {
            'package': 'angle/traces/words_crush',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_words_crush',
  },
  'src/tests/restricted_traces/words_of_wonders': {
      'packages': [
        {
            'package': 'angle/traces/words_of_wonders',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_words_of_wonders',
  },
  'src/tests/restricted_traces/words_with_friends_2': {
      'packages': [
        {
            'package': 'angle/traces/words_with_friends_2',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_words_with_friends_2',
  },
  'src/tests/restricted_traces/wordscapes': {
      'packages': [
        {
            'package': 'angle/traces/wordscapes',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_wordscapes',
  },
  'src/tests/restricted_traces/world_cricket_championship_2': {
      'packages': [
        {
            'package': 'angle/traces/world_cricket_championship_2',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_world_cricket_championship_2',
  },
  'src/tests/restricted_traces/world_of_kings': {
      'packages': [
        {
            'package': 'angle/traces/world_of_kings',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_world_of_kings',
  },
  'src/tests/restricted_traces/world_of_tanks_blitz': {
      'packages': [
        {
            'package': 'angle/traces/world_of_tanks_blitz',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_world_of_tanks_blitz',
  },
  'src/tests/restricted_traces/world_war_doh': {
      'packages': [
        {
            'package': 'angle/traces/world_war_doh',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_world_war_doh',
  },
  'src/tests/restricted_traces/worms_zone_io': {
      'packages': [
        {
            'package': 'angle/traces/worms_zone_io',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_worms_zone_io',
  },
  'src/tests/restricted_traces/zenonia_4': {
      'packages': [
        {
            'package': 'angle/traces/zenonia_4',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_zenonia_4',
  },
  'src/tests/restricted_traces/zillow': {
      'packages': [
        {
            'package': 'angle/traces/zillow',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_zillow',
  },
  'src/tests/restricted_traces/zombie_smasher': {
      'packages': [
        {
            'package': 'angle/traces/zombie_smasher',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_zombie_smasher',
  },
  # === ANGLE Restricted Trace Generated Code End ===

  # === ANDROID_DEPS Generated Code Start ===
  # Generated by //third_party/android_deps/fetch_all.py
  'third_party/android_deps/libs/android_arch_core_common': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/android_arch_core_common',
              'version': 'version:2@1.1.1.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/android_arch_core_runtime': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/android_arch_core_runtime',
              'version': 'version:2@1.1.1.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/android_arch_lifecycle_common': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/android_arch_lifecycle_common',
              'version': 'version:2@1.1.1.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/android_arch_lifecycle_livedata': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/android_arch_lifecycle_livedata',
              'version': 'version:2@1.1.1.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/android_arch_lifecycle_livedata_core': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/android_arch_lifecycle_livedata_core',
              'version': 'version:2@1.1.1.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/android_arch_lifecycle_runtime': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/android_arch_lifecycle_runtime',
              'version': 'version:2@1.1.1.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/android_arch_lifecycle_viewmodel': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/android_arch_lifecycle_viewmodel',
              'version': 'version:2@1.1.1.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_android_support_asynclayoutinflater': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_support_asynclayoutinflater',
              'version': 'version:2@28.0.0.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_android_support_collections': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_support_collections',
              'version': 'version:2@28.0.0.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_android_support_coordinatorlayout': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_support_coordinatorlayout',
              'version': 'version:2@28.0.0.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_android_support_cursoradapter': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_support_cursoradapter',
              'version': 'version:2@28.0.0.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_android_support_customview': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_support_customview',
              'version': 'version:2@28.0.0.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_android_support_documentfile': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_support_documentfile',
              'version': 'version:2@28.0.0.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_android_support_drawerlayout': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_support_drawerlayout',
              'version': 'version:2@28.0.0.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_android_support_interpolator': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_support_interpolator',
              'version': 'version:2@28.0.0.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_android_support_loader': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_support_loader',
              'version': 'version:2@28.0.0.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_android_support_localbroadcastmanager': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_support_localbroadcastmanager',
              'version': 'version:2@28.0.0.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_android_support_print': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_support_print',
              'version': 'version:2@28.0.0.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_android_support_slidingpanelayout': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_support_slidingpanelayout',
              'version': 'version:2@28.0.0.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_android_support_support_annotations': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_support_support_annotations',
              'version': 'version:2@28.0.0.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_android_support_support_compat': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_support_support_compat',
              'version': 'version:2@28.0.0.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_android_support_support_core_ui': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_support_support_core_ui',
              'version': 'version:2@28.0.0.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_android_support_support_core_utils': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_support_support_core_utils',
              'version': 'version:2@28.0.0.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_android_support_swiperefreshlayout': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_support_swiperefreshlayout',
              'version': 'version:2@28.0.0.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_android_support_versionedparcelable': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_support_versionedparcelable',
              'version': 'version:2@28.0.0.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_android_support_viewpager': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_support_viewpager',
              'version': 'version:2@28.0.0.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_android_tools_common': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_tools_common',
              'version': 'version:2@30.2.0-beta01.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_android_tools_layoutlib_layoutlib_api': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_tools_layoutlib_layoutlib_api',
              'version': 'version:2@30.2.0-beta01.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_android_tools_sdk_common': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_tools_sdk_common',
              'version': 'version:2@30.2.0-beta01.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_github_ben_manes_caffeine_caffeine': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_github_ben_manes_caffeine_caffeine',
              'version': 'version:2@2.8.8.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_github_kevinstern_software_and_algorithms': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_github_kevinstern_software_and_algorithms',
              'version': 'version:2@1.0.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_google_android_annotations': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_android_annotations',
              'version': 'version:2@4.1.1.4.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_google_android_apps_common_testing_accessibility_framework_accessibility_test_framework': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_android_apps_common_testing_accessibility_framework_accessibility_test_framework',
              'version': 'version:2@4.0.0.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_google_android_datatransport_transport_api': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_android_datatransport_transport_api',
              'version': 'version:2@2.2.1.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_google_android_gms_play_services_auth': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_android_gms_play_services_auth',
              'version': 'version:2@20.1.0.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_google_android_gms_play_services_auth_api_phone': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_android_gms_play_services_auth_api_phone',
              'version': 'version:2@18.0.1.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_google_android_gms_play_services_auth_base': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_android_gms_play_services_auth_base',
              'version': 'version:2@18.0.2.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_google_android_gms_play_services_base': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_android_gms_play_services_base',
              'version': 'version:2@18.1.0.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_google_android_gms_play_services_basement': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_android_gms_play_services_basement',
              'version': 'version:2@18.1.0.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_google_android_gms_play_services_cast': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_android_gms_play_services_cast',
              'version': 'version:2@17.0.0.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_google_android_gms_play_services_cast_framework': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_android_gms_play_services_cast_framework',
              'version': 'version:2@17.0.0.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_google_android_gms_play_services_clearcut': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_android_gms_play_services_clearcut',
              'version': 'version:2@17.0.0.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_google_android_gms_play_services_cloud_messaging': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_android_gms_play_services_cloud_messaging',
              'version': 'version:2@16.0.0.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_google_android_gms_play_services_flags': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_android_gms_play_services_flags',
              'version': 'version:2@17.0.0.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_google_android_gms_play_services_gcm': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_android_gms_play_services_gcm',
              'version': 'version:2@17.0.0.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_google_android_gms_play_services_iid': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_android_gms_play_services_iid',
              'version': 'version:2@17.0.0.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_google_android_gms_play_services_instantapps': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_android_gms_play_services_instantapps',
              'version': 'version:2@18.0.1.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_google_android_gms_play_services_location': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_android_gms_play_services_location',
              'version': 'version:2@21.0.1.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_google_android_gms_play_services_phenotype': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_android_gms_play_services_phenotype',
              'version': 'version:2@17.0.0.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_google_android_gms_play_services_stats': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_android_gms_play_services_stats',
              'version': 'version:2@17.0.0.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_google_android_gms_play_services_tasks': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_android_gms_play_services_tasks',
              'version': 'version:2@18.0.2.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_google_android_gms_play_services_vision': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_android_gms_play_services_vision',
              'version': 'version:2@20.1.3.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_google_android_gms_play_services_vision_common': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_android_gms_play_services_vision_common',
              'version': 'version:2@19.1.3.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_google_android_material_material': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_android_material_material',
              'version': 'version:2@1.11.0-beta01.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_google_android_play_core_common': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_android_play_core_common',
              'version': 'version:2@2.0.2.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_google_android_play_feature_delivery': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_android_play_feature_delivery',
              'version': 'version:2@2.0.1.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_google_auto_auto_common': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_auto_auto_common',
              'version': 'version:2@1.2.1.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_google_auto_service_auto_service': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_auto_service_auto_service',
              'version': 'version:2@1.0-rc6.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_google_auto_service_auto_service_annotations': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_auto_service_auto_service_annotations',
              'version': 'version:2@1.0-rc6.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_google_auto_value_auto_value_annotations': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_auto_value_auto_value_annotations',
              'version': 'version:2@1.10.4.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_google_code_findbugs_jsr305': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_code_findbugs_jsr305',
              'version': 'version:2@3.0.2.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_google_code_gson_gson': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_code_gson_gson',
              'version': 'version:2@2.9.0.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_google_dagger_dagger': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_dagger_dagger',
              'version': 'version:2@2.30.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_google_dagger_dagger_compiler': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_dagger_dagger_compiler',
              'version': 'version:2@2.30.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_google_dagger_dagger_producers': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_dagger_dagger_producers',
              'version': 'version:2@2.30.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_google_dagger_dagger_spi': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_dagger_dagger_spi',
              'version': 'version:2@2.30.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_google_errorprone_error_prone_annotation': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_errorprone_error_prone_annotation',
              'version': 'version:2@2.19.1.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_google_errorprone_error_prone_annotations': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_errorprone_error_prone_annotations',
              'version': 'version:2@2.23.0.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_google_errorprone_error_prone_check_api': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_errorprone_error_prone_check_api',
              'version': 'version:2@2.11.0.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_google_errorprone_error_prone_core': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_errorprone_error_prone_core',
              'version': 'version:2@2.11.0.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_google_errorprone_error_prone_type_annotations': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_errorprone_error_prone_type_annotations',
              'version': 'version:2@2.11.0.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_google_errorprone_javac': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_errorprone_javac',
              'version': 'version:2@9+181-r4173-1.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_google_errorprone_javac_shaded': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_errorprone_javac_shaded',
              'version': 'version:2@9-dev-r4023-3.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_google_firebase_firebase_annotations': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_firebase_firebase_annotations',
              'version': 'version:2@16.0.0.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_google_firebase_firebase_common': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_firebase_firebase_common',
              'version': 'version:2@19.5.0.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_google_firebase_firebase_components': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_firebase_firebase_components',
              'version': 'version:2@16.1.0.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_google_firebase_firebase_encoders': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_firebase_firebase_encoders',
              'version': 'version:2@16.1.0.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_google_firebase_firebase_encoders_json': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_firebase_firebase_encoders_json',
              'version': 'version:2@17.1.0.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_google_firebase_firebase_iid': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_firebase_firebase_iid',
              'version': 'version:2@21.0.1.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_google_firebase_firebase_iid_interop': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_firebase_firebase_iid_interop',
              'version': 'version:2@17.0.0.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_google_firebase_firebase_installations': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_firebase_firebase_installations',
              'version': 'version:2@16.3.5.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_google_firebase_firebase_installations_interop': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_firebase_firebase_installations_interop',
              'version': 'version:2@16.0.1.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_google_firebase_firebase_measurement_connector': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_firebase_firebase_measurement_connector',
              'version': 'version:2@18.0.0.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_google_firebase_firebase_messaging': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_firebase_firebase_messaging',
              'version': 'version:2@21.0.1.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_google_googlejavaformat_google_java_format': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_googlejavaformat_google_java_format',
              'version': 'version:2@1.5.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_google_guava_failureaccess': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_guava_failureaccess',
              'version': 'version:2@1.0.1.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_google_guava_guava': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_guava_guava',
              'version': 'version:2@32.1.3-jre.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_google_guava_guava_android': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_guava_guava_android',
              'version': 'version:2@32.1.3-android.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_google_j2objc_j2objc_annotations': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_j2objc_j2objc_annotations',
              'version': 'version:2@2.8.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_google_protobuf_protobuf_java': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_protobuf_protobuf_java',
              'version': 'version:2@3.19.2.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_google_protobuf_protobuf_javalite': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_protobuf_protobuf_javalite',
              'version': 'version:2@3.21.1.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_googlecode_java_diff_utils_diffutils': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_googlecode_java_diff_utils_diffutils',
              'version': 'version:2@1.3.0.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_squareup_javapoet': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_squareup_javapoet',
              'version': 'version:2@1.13.0.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_squareup_javawriter': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_squareup_javawriter',
              'version': 'version:2@2.1.1.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_squareup_moshi_moshi': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_squareup_moshi_moshi',
              'version': 'version:2@1.15.0.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_squareup_moshi_moshi_adapters': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_squareup_moshi_moshi_adapters',
              'version': 'version:2@1.15.0.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_squareup_okio_okio_jvm': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_squareup_okio_okio_jvm',
              'version': 'version:2@3.7.0.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_squareup_wire_wire_runtime_jvm': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_squareup_wire_wire_runtime_jvm',
              'version': 'version:2@4.9.7.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/io_github_java_diff_utils_java_diff_utils': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/io_github_java_diff_utils_java_diff_utils',
              'version': 'version:2@4.0.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/io_grpc_grpc_api': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/io_grpc_grpc_api',
              'version': 'version:2@1.49.0.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/io_grpc_grpc_binder': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/io_grpc_grpc_binder',
              'version': 'version:2@1.49.0.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/io_grpc_grpc_context': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/io_grpc_grpc_context',
              'version': 'version:2@1.49.0.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/io_grpc_grpc_core': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/io_grpc_grpc_core',
              'version': 'version:2@1.49.0.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/io_grpc_grpc_protobuf_lite': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/io_grpc_grpc_protobuf_lite',
              'version': 'version:2@1.49.0.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/io_grpc_grpc_stub': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/io_grpc_grpc_stub',
              'version': 'version:2@1.49.0.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/io_perfmark_perfmark_api': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/io_perfmark_perfmark_api',
              'version': 'version:2@0.25.0.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/javax_annotation_javax_annotation_api': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/javax_annotation_javax_annotation_api',
              'version': 'version:2@1.3.2.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/javax_annotation_jsr250_api': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/javax_annotation_jsr250_api',
              'version': 'version:2@1.0.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/javax_inject_javax_inject': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/javax_inject_javax_inject',
              'version': 'version:2@1.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/net_bytebuddy_byte_buddy': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/net_bytebuddy_byte_buddy',
              'version': 'version:2@1.14.12.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/net_bytebuddy_byte_buddy_agent': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/net_bytebuddy_byte_buddy_agent',
              'version': 'version:2@1.14.12.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/net_ltgt_gradle_incap_incap': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/net_ltgt_gradle_incap_incap',
              'version': 'version:2@0.2.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/org_bouncycastle_bcprov_jdk18on': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_bouncycastle_bcprov_jdk18on',
              'version': 'version:2@1.77.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/org_ccil_cowan_tagsoup_tagsoup': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_ccil_cowan_tagsoup_tagsoup',
              'version': 'version:2@1.2.1.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/org_checkerframework_checker_compat_qual': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_checkerframework_checker_compat_qual',
              'version': 'version:2@2.5.5.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/org_checkerframework_checker_qual': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_checkerframework_checker_qual',
              'version': 'version:2@3.37.0.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/org_checkerframework_checker_util': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_checkerframework_checker_util',
              'version': 'version:2@3.25.0.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/org_checkerframework_dataflow_errorprone': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_checkerframework_dataflow_errorprone',
              'version': 'version:2@3.15.0.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/org_codehaus_mojo_animal_sniffer_annotations': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_codehaus_mojo_animal_sniffer_annotations',
              'version': 'version:2@1.21.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/org_conscrypt_conscrypt_openjdk_uber': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_conscrypt_conscrypt_openjdk_uber',
              'version': 'version:2@2.5.2.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/org_eclipse_jgit_org_eclipse_jgit': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_eclipse_jgit_org_eclipse_jgit',
              'version': 'version:2@4.4.1.201607150455-r.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/org_hamcrest_hamcrest': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_hamcrest_hamcrest',
              'version': 'version:2@2.2.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/org_jetbrains_kotlin_kotlin_android_extensions_runtime': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_jetbrains_kotlin_kotlin_android_extensions_runtime',
              'version': 'version:2@1.9.22.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/org_jetbrains_kotlin_kotlin_parcelize_runtime': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_jetbrains_kotlin_kotlin_parcelize_runtime',
              'version': 'version:2@1.9.22.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/org_jetbrains_kotlin_kotlin_stdlib_jdk7': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_jetbrains_kotlin_kotlin_stdlib_jdk7',
              'version': 'version:2@1.8.20.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/org_jetbrains_kotlin_kotlin_stdlib_jdk8': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_jetbrains_kotlin_kotlin_stdlib_jdk8',
              'version': 'version:2@1.8.20.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/org_jetbrains_kotlinx_atomicfu_jvm': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_jetbrains_kotlinx_atomicfu_jvm',
              'version': 'version:2@0.23.2.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/org_jetbrains_kotlinx_kotlinx_coroutines_android': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_jetbrains_kotlinx_kotlinx_coroutines_android',
              'version': 'version:2@1.6.4.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/org_jetbrains_kotlinx_kotlinx_coroutines_core_jvm': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_jetbrains_kotlinx_kotlinx_coroutines_core_jvm',
              'version': 'version:2@1.6.4.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/org_jetbrains_kotlinx_kotlinx_coroutines_guava': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_jetbrains_kotlinx_kotlinx_coroutines_guava',
              'version': 'version:2@1.6.4.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/org_jetbrains_kotlinx_kotlinx_metadata_jvm': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_jetbrains_kotlinx_kotlinx_metadata_jvm',
              'version': 'version:2@0.1.0.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/org_jsoup_jsoup': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_jsoup_jsoup',
              'version': 'version:2@1.15.1.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/org_mockito_mockito_android': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_mockito_mockito_android',
              'version': 'version:2@5.11.0.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/org_mockito_mockito_core': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_mockito_mockito_core',
              'version': 'version:2@5.11.0.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/org_mockito_mockito_subclass': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_mockito_mockito_subclass',
              'version': 'version:2@5.11.0.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/org_objenesis_objenesis': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_objenesis_objenesis',
              'version': 'version:2@3.3.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/org_ow2_asm_asm': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_ow2_asm_asm',
              'version': 'version:2@9.7.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/org_ow2_asm_asm_analysis': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_ow2_asm_asm_analysis',
              'version': 'version:2@9.7.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/org_ow2_asm_asm_commons': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_ow2_asm_asm_commons',
              'version': 'version:2@9.7.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/org_ow2_asm_asm_tree': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_ow2_asm_asm_tree',
              'version': 'version:2@9.7.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/org_ow2_asm_asm_util': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_ow2_asm_asm_util',
              'version': 'version:2@9.7.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/org_pcollections_pcollections': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_pcollections_pcollections',
              'version': 'version:2@3.1.4.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/org_robolectric_annotations': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_robolectric_annotations',
              'version': 'version:2@4.12.1.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/org_robolectric_junit': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_robolectric_junit',
              'version': 'version:2@4.12.1.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/org_robolectric_nativeruntime': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_robolectric_nativeruntime',
              'version': 'version:2@4.12.1.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/org_robolectric_nativeruntime_dist_compat': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_robolectric_nativeruntime_dist_compat',
              'version': 'version:2@1.0.9.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/org_robolectric_pluginapi': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_robolectric_pluginapi',
              'version': 'version:2@4.12.1.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/org_robolectric_plugins_maven_dependency_resolver': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_robolectric_plugins_maven_dependency_resolver',
              'version': 'version:2@4.12.1.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/org_robolectric_resources': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_robolectric_resources',
              'version': 'version:2@4.12.1.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/org_robolectric_robolectric': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_robolectric_robolectric',
              'version': 'version:2@4.12.1.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/org_robolectric_sandbox': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_robolectric_sandbox',
              'version': 'version:2@4.12.1.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/org_robolectric_shadowapi': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_robolectric_shadowapi',
              'version': 'version:2@4.12.1.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/org_robolectric_shadows_framework': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_robolectric_shadows_framework',
              'version': 'version:2@4.12.1.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/org_robolectric_shadows_versioning': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_robolectric_shadows_versioning',
              'version': 'version:2@4.12.1.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/org_robolectric_utils': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_robolectric_utils',
              'version': 'version:2@4.12.1.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/org_robolectric_utils_reflector': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_robolectric_utils_reflector',
              'version': 'version:2@4.12.1.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  # === ANDROID_DEPS Generated Code End ===
}

hooks = [
  {
    # Ensure that the DEPS'd "depot_tools" has its self-update capability
    # disabled.
    'name': 'disable_depot_tools_selfupdate',
    'pattern': '.',
    'condition': 'not build_with_chromium',
    'action': [
        'python3',
        'third_party/depot_tools/update_depot_tools_toggle.py',
        '--disable',
    ],
  },
  {
    'name': 'sysroot_x86',
    'pattern': '.',
    'condition': 'checkout_linux and ((checkout_x86 or checkout_x64) and not build_with_chromium)',
    'action': ['python3', 'build/linux/sysroot_scripts/install-sysroot.py',
               '--arch=x86'],
  },
  {
    'name': 'sysroot_x64',
    'pattern': '.',
    'condition': 'checkout_linux and (checkout_x64 and not build_with_chromium)',
    'action': ['python3', 'build/linux/sysroot_scripts/install-sysroot.py',
               '--arch=x64'],
  },
  {
    # Case-insensitivity for the Win SDK. Must run before win_toolchain below.
    'name': 'ciopfs_linux',
    'pattern': '.',
    'condition': 'checkout_win and host_os == "linux" and not build_with_chromium',
    'action': [ 'python3',
                'third_party/depot_tools/download_from_google_storage.py',
                '--no_resume',
                '--no_auth',
                '--bucket', 'chromium-browser-clang/ciopfs',
                '-s', 'build/ciopfs.sha1',
    ]
  },
  {
    # Update the Windows toolchain if necessary.  Must run before 'clang' below.
    'name': 'win_toolchain',
    'pattern': '.',
    'condition': 'checkout_win and not build_with_chromium',
    'action': ['python3', 'build/vs_toolchain.py', 'update', '--force'],
  },
  {
    # Update the Mac toolchain if necessary.
    'name': 'mac_toolchain',
    'pattern': '.',
    'condition': 'checkout_mac and not build_with_chromium',
    'action': ['python3', 'build/mac_toolchain.py'],
  },

  {
    # Note: On Win, this should run after win_toolchain, as it may use it.
    'name': 'clang',
    'pattern': '.',
    'action': ['python3', 'tools/clang/scripts/update.py'],
    'condition': 'not build_with_chromium',
  },

  {
    # Update LASTCHANGE.
    'name': 'lastchange',
    'pattern': '.',
    'condition': 'not build_with_chromium',
    'action': ['python3', 'build/util/lastchange.py',
               '-o', 'build/util/LASTCHANGE'],
  },

  # Pull rc binaries using checked-in hashes.
  {
    'name': 'rc_win',
    'pattern': '.',
    'condition': 'checkout_win and host_os == "win" and not build_with_chromium',
    'action': [ 'python3',
                'third_party/depot_tools/download_from_google_storage.py',
                '--no_resume',
                '--no_auth',
                '--bucket', 'chromium-browser-clang/rc',
                '-s', 'build/toolchain/win/rc/win/rc.exe.sha1',
    ],
  },

  {
    'name': 'rc_mac',
    'pattern': '.',
    'condition': 'checkout_win and host_os == "mac" and not build_with_chromium',
    'action': [ 'python3',
                'third_party/depot_tools/download_from_google_storage.py',
                '--no_resume',
                '--no_auth',
                '--bucket', 'chromium-browser-clang/rc',
                '-s', 'build/toolchain/win/rc/mac/rc.sha1',
    ],
  },
  {
    'name': 'rc_linux',
    'pattern': '.',
    'condition': 'checkout_win and host_os == "linux" and not build_with_chromium',
    'action': [ 'python3',
                'third_party/depot_tools/download_from_google_storage.py',
                '--no_resume',
                '--no_auth',
                '--bucket', 'chromium-browser-clang/rc',
                '-s', 'build/toolchain/win/rc/linux64/rc.sha1',
    ]
  },

  # Download glslang validator binary for Linux.
  {
    'name': 'linux_glslang_validator',
    'pattern': '.',
    'condition': 'checkout_linux and not build_with_chromium',
    'action': [ 'python3',
                'third_party/depot_tools/download_from_google_storage.py',
                '--no_resume',
                '--platform=linux*',
                '--no_auth',
                '--bucket', 'angle-glslang-validator',
                '-s', 'tools/glslang/glslang_validator.sha1',
    ],
  },

  # Download glslang validator binary for Windows.
  {
    'name': 'win_glslang_validator',
    'pattern': '.',
    'condition': 'checkout_win and not build_with_chromium',
    'action': [ 'python3',
                'third_party/depot_tools/download_from_google_storage.py',
                '--no_resume',
                '--platform=win32*',
                '--no_auth',
                '--bucket', 'angle-glslang-validator',
                '-s', 'tools/glslang/glslang_validator.exe.sha1',
    ],
  },

  # Download flex/bison binaries for Linux.
  {
    'name': 'linux_flex_bison',
    'pattern': '.',
    'condition': 'checkout_linux and not build_with_chromium',
    'action': [ 'python3',
                'third_party/depot_tools/download_from_google_storage.py',
                '--no_resume',
                '--platform=linux*',
                '--no_auth',
                '--bucket', 'angle-flex-bison',
                '-d', 'tools/flex-bison/linux/',
    ],
  },

  # Download flex/bison binaries for Windows.
  {
    'name': 'win_flex_bison',
    'pattern': '.',
    'condition': 'checkout_win and not build_with_chromium',
    'action': [ 'python3',
                'third_party/depot_tools/download_from_google_storage.py',
                '--no_resume',
                '--platform=win32*',
                '--no_auth',
                '--bucket', 'angle-flex-bison',
                '-d', 'tools/flex-bison/windows/',
    ],
  },

  # Set up an input file for the Mesa setup step.
  {
    'name': 'mesa_input',
    'pattern': '.',
    'condition': 'checkout_angle_mesa',
    'action': [ 'python3', 'third_party/mesa/mesa_build.py', 'runhook', ],
  },

  # Configure remote exec cfg files
  {
    'name': 'configure_reclient_cfgs',
    'pattern': '.',
    'condition': 'download_remoteexec_cfg and not build_with_chromium and not (host_os == "linux" and host_cpu == "arm64")',
    'action': ['python3',
               'buildtools/reclient_cfgs/configure_reclient_cfgs.py',
               '--rbe_instance',
               Var('rbe_instance'),
               '--reproxy_cfg_template',
               'reproxy.cfg.template',
               '--rewrapper_cfg_project',
               Var('rewrapper_cfg_project'),
               '--quiet',
               ],
  },
  # Configure Siso for developer builds.
  {
    'name': 'configure_siso',
    'pattern': '.',
    'condition': 'not build_with_chromium',
    'action': ['python3',
               'build/config/siso/configure_siso.py',
               '--rbe_instance',
               Var('rbe_instance'),
               ],
  },
]

recursedeps = [
  'buildtools',
  'third_party/googletest',
  'third_party/jsoncpp',
  'third_party/dawn',
]

skip_child_includes = [
    'third_party'
]
