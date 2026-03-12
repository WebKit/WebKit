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
  'chromium_revision': '64d19a776fb7422e66dda4b121dcb33c56be379d',
  # We never want to checkout chromium,
  # but need a dummy DEPS entry for the autoroller
  'dummy_checkout_chromium': False,

  # Current revision of VK-GL-CTS (a.k.a dEQP).
  'vk_gl_cts_revision': '4814d9cefb63c4a0f728b02373509d86988e097b',

  # Current revision of googletest.
  'googletest_revision': '4fe3307fb2d9f86d19777c7eb0e4809e9694dde7',

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
  'catapult_revision': 'ccdc7385a8e9f12b1b1bdfaf18be2319f49cea26',

  # the commit queue can handle CLs rolling Fuchsia sdk
  # and whatever else without interference from each other.
  'fuchsia_version': 'version:30.20251126.3.1',

  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling luci-go
  # and whatever else without interference from each other.
  'luci_go': 'git_revision:7defb40e58ef45f23d43fbd6494aae4a8a38435b',

  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling android_sdk_build-tools_version
  # and whatever else without interference from each other.
  'android_sdk_build-tools_version': 'y3EsZLg4bxPmpW0oYsAHylywNyMnIwPS3kh1VbQLAFAC',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling android_sdk_emulator_version
  # and whatever else without interference from each other.
  'android_sdk_emulator_version': '9lGp8nTUCRRWGMnI_96HcKfzjnxEJKUcfvfwmA3wXNkC',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling android_sdk_platform-tools_version
  # and whatever else without interference from each other.
  'android_sdk_platform-tools_version': 'qTD9QdBlBf3dyHsN1lJ0RH6AhHxR42Hmg2Ih-Vj4zIEC',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling android_sdk_platforms_version
  # and whatever else without interference from each other.
  'android_sdk_platforms_version': '_YHemUrK49JrE7Mctdf5DDNOHu1VKBx_PTcWnZ-cbOAC',

  # ninja CIPD package version.
  # https://chrome-infra-packages.appspot.com/p/infra/3pp/tools/ninja
  'ninja_version': 'version:3@1.12.1.chromium.4',

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
  'reclient_version': 're_client_version:0.185.0.db415f21-gomaip',

  # siso CIPD package version.
  'siso_version': 'git_revision:03ee208f9c31a303e1ba61f9bc7219158078bd50',

  # 'magic' text to tell depot_tools that git submodules should be accepted but
  # but parity with DEPS file is expected.
  'SUBMODULE_MIGRATION': 'True',

  # condition to allowlist deps to be synced in Cider. Allowlisting is needed
  # because not all deps are compatible with Cider. Once we migrate everything
  # to be compatible we can get rid of this allowlisting mecahnism and remove
  # this condition. Tracking bug for removing this condition: b/349365433
  'non_git_source': 'True',

   # Make Dawn skip its standalone dependencies
  'dawn_standalone': False,
  'dawn_revision': 'c1308bb3f17e9637f82be72f7ed0a75f3427dda4',

  # All of the restricted traces (large).
  'checkout_angle_restricted_traces': 'checkout_angle_internal',

  # Individual vars to enable/disable checkout of only specific traces.
  # === ANGLE Restricted Trace Generated Var Start ===
  'checkout_angle_restricted_trace_1945_air_force': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_20_minutes_till_dawn': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_2_3_4_player_mini_games': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_3d_pool_ball': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_ace_racer': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_afk_arena': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_age_of_origins_z': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_agent_a': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_aliexpress': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_altos_odyssey': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_among_us': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_angry_birds_2_1500': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_angry_birds_2_launch': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_animal_crossing': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_antistress': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_antutu_refinery': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_apex_girl': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_archero_2': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_arena_of_valor': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_arknights': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_asphalt_8': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_asphalt_9': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_asphalt_9_2024': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_avakin_life': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_avatar_world': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_aztec_ruins': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_aztec_ruins_high': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_badland': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_balatro': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_basemark_gpu': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_batman_telltale': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_battle_of_polytopia': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_beach_buggy_racing': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_billiards_city': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_bingo_blitz': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_black_beacon': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_black_clover_m': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_black_desert_mobile': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_blade_and_soul_revolution': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_block_blast': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_block_craft_3d': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_blockman_go': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_botworld_adventure': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_brain_test_tricky_puzzles': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_brawl_stars': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_brawlhalla': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_bricks_breaker_quest': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_bridge_constructor_portal': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_brotato': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_bubble_shooter': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_bubble_shooter_and_friends': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_bubble_shooter_relaxing': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_bullet_echo': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_bus_simulator_indonesia': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_call_break_offline_card_game': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_callbreak': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_candy_crush_500': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_candy_crush_soda_saga': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_car_chase': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_car_parking_multiplayer': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_car_race_3d': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_carrom_pool_disc_game': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_cash_frenzy_casino_slots': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_cashman_casino': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_castlevania_sotn': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_cat_runner': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_catalyst_black': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_chrono_trigger': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_clash_of_clans': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_clash_royale': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_cod_mobile': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_coin_master': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_color_block_combo_blast': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_command_and_conquer_rivals': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_cookie_run_kingdom': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_cookie_run_oven_break': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_critical_ops': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_csr2_drag_racing': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_cut_the_rope': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_dark_war_survival': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_darkness_rises': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_dc_dark_legion': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_dead_by_daylight': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_dead_cells': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_dead_trigger_2': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_diablo_immortal': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_dice_dreams': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_disney_mirrorverse': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_disney_solitaire': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_disney_tsum_tsum': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_domino_dreams': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_dont_starve': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_dota_underlords': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_downwell': 'checkout_angle_restricted_traces',
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
  'checkout_angle_restricted_trace_fashion_show': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_fate_grand_order': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_fifa_mobile': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_final_fantasy': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_final_fantasy_brave_exvius': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_fire_emblem_heroes': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_fishdom': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_five_nights_at_freddys': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_football_league_2025': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_free_fire': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_free_fire_max': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_frost_and_flame': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_gacha_club': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_gacha_life': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_gangstar_vegas': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_gardenscapes': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_genshin_impact': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_geometry_dash': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_goddess_of_victory_nikke': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_going_balls': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_google_maps': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_gossip_harbor': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_grand_mountain_adventure': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_grimvalor': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_gwent_witcher_card_game': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_happy_color': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_harry_potter_hogwarts_mystery': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_harry_potter_magic_awakened': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_hay_day': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_hearthstone': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_hero_wars': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_higgs_domino_island': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_hill_climb_racing': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_hole_io': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_homescapes': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_honkai_impact_3rd': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_honkai_star_rail': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_hungry_shark_evolution': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_hunter_assassin': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_i9_inferno_nine': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_identity_v': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_idle_heroes': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_indian_bikes_driving_3d': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_infinity_ops': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_injustice_2': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_into_the_dead_2': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_jackpot_party': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_jackpot_world': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_jetpack_joyride': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_junes_journey': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_kartrider_rush': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_kentucky_route_zero': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_kingshot': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_klondike_adventures': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_knives_out': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_lands_of_jail': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_last_shelter_survival': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_last_war_survival_game': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_last_z_survival_shooter': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_league_of_legends_wild_rift': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_lego_legacy': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_life_is_strange': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_lightning_link': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_lilys_garden': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_limbo': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_lineage_2m': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_lineage_m': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_loop_hero': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_lords_mobile': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_lotsa_slots': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_love_and_deepspace': 'checkout_angle_restricted_traces',
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
  'checkout_angle_restricted_trace_match_factory': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_match_masters': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_matching_story': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_merge_dragons': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_merge_mansion': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_messenger_lite': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_minecraft': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_minecraft_bedrock': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_minecraft_vibrant_visuals': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_minetest': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_mini_block_craft': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_mini_games_calm_and_relax': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_mini_world': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_mob_control': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_mobile_legends': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_modern_combat_5': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_monopoly_go': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_monster_hunter_stories': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_monster_strike': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_monument_valley': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_mortal_kombat': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_mu_origin_3': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_my_friend_pedro': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_my_talking_angela': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_my_talking_angela_2': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_my_talking_tom': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_my_talking_tom2': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_my_talking_tom_friends': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_nba2k20_800': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_need_for_speed_no_limits': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_new_legend_of_the_condor_heroes': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_ni_no_kuni': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_nier_reincarnation': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_octopath_traveler': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_odin_valhalla_rising': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_off_the_road': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_offline_games': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_one_piece_treasure_cruise': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_one_punch_man': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_onmyoji': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_oxenfree': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_paper_io2': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_passmark_complex': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_passmark_simple': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_passmark_unity': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_piano_fire': 'checkout_angle_restricted_traces',
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
  'checkout_angle_restricted_trace_puzzle_and_dragons': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_puzzles_and_survival': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_qblock_wood_block_puzzle_game': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_race_master_3d': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_ragnarok_m_eternal_love': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_raid_shadow_legends': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_raising_a_goblin': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_real_commando_secret_mission': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_real_cricket_20': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_real_gangster_crime': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_real_racing3': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_respawnables': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_retro_bowl': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_return_to_monkey_island': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_riptide_gp2': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_rise_of_empires': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_rise_of_kingdoms': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_romancing_saga': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_rope_hero_vice_town': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_royal_kingdom': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_royal_match': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_rush_royale': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_saint_seiya_awakening': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_sakura_school_simulator': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_scary_teacher_3d': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_school_party_craft': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_scrabble_go': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_seaside_escape': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_shadow_fight_2': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_shadow_fight_3': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_shadowgun_legends': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_shovel_knight_pocket_dungeon': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_simcity_buildit': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_sky_force_reloaded': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_slam_dunk_from_tv_animation': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_slay_the_spire': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_slingshot_test1': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_slingshot_test2': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_slotomania': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_snake_io': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_snake_lite': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_sniper_3d': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_soccer_superstar': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_solar_smash': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_sonic_forces': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_sonic_the_hedgehog': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_special_forces_group_2': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_spider_fighter_3': 'checkout_angle_restricted_traces',
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
  'checkout_angle_restricted_trace_super_bear_adventure': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_super_mario_run': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_supertuxkart': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_supreme_duelist': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_survivor_io': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_talking_tom_gold_run': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_talking_tom_hero_dash': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_temple_run_2': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_temple_run_300': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_tesla': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_teslagrad': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_tessellation': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_the_gardens_between': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_the_sims_mobile': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_the_witcher_monster_slayer': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_thief_puzzle': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_thimbleweed_park': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_tile_club': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_tmnt_shredders_revenge': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_toca_life_world': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_toon_blast': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_top_heroes_kingdom_saga': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_top_war': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_total_battle': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_tower_of_fantasy': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_township': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_traffic_rider': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_travel_town': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_trex_200': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_two_player_games': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_uber': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_umamusume_pretty_derby': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_vainglory': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_vita_mahjong': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_walking_dead_survivors': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_war_planet_online': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_warcraft_rumble': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_warhammer_40000_freeblade': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_wayward_souls': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_whatsapp': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_whiteout_survival': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_woodoku': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_words_crush': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_words_of_wonders': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_words_with_friends_2': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_wordscapes': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_world_cricket_championship_2': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_world_of_kings': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_world_of_tanks_blitz': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_world_war_doh': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_worms_zone_io': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_wuthering_waves': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_yalla_ludo': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_zenonia_4': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_zillow': 'checkout_angle_restricted_traces',
  'checkout_angle_restricted_trace_zombie_smasher': 'checkout_angle_restricted_traces',
  # === ANGLE Restricted Trace Generated Var End ===

  'checkout_angle_perfetto': 'checkout_angle_restricted_traces',
}

deps = {

  'build': {
    'url': Var('chromium_git') + '/chromium/src/build.git@a200a59ced34de9a35fa70ce20af464c37091401',
    'condition': 'not build_with_chromium',
  },

  'buildtools': {
    'url': Var('chromium_git') + '/chromium/src/buildtools.git@629078f56b024d17eb0588c7bdbf3bcafb5bff00',
    'condition': 'not build_with_chromium',
  },

  'third_party/clang-format/script': {
    'url': Var('chromium_git') + '/external/github.com/llvm/llvm-project/clang/tools/clang-format.git@c2725e0622e1a86d55f14514f2177a39efea4a0e',
    'condition': 'not build_with_chromium',
  },

  'buildtools/linux64': {
    'packages': [
      {
        'package': 'gn/gn/linux-${{arch}}',
        'version': 'git_revision:4619125bd337d259c0dc9f958d0102adc99d2543',
      }
    ],
    'dep_type': 'cipd',
    'condition': 'not build_with_chromium and checkout_linux',
  },

  'buildtools/mac': {
    'packages': [
      {
        'package': 'gn/gn/mac-${{arch}}',
        'version': 'git_revision:4619125bd337d259c0dc9f958d0102adc99d2543',
      }
    ],
    'dep_type': 'cipd',
    'condition': 'not build_with_chromium and checkout_mac',
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
        'version': 'git_revision:4619125bd337d259c0dc9f958d0102adc99d2543',
      }
    ],
    'dep_type': 'cipd',
    'condition': 'not build_with_chromium and checkout_win',
  },

  'testing': {
    'url': '{chromium_git}/chromium/src/testing@2576331a434cbc0b33feb746577b6e0354ec1e5b',
    'condition': 'not build_with_chromium',
  },

  'third_party/abseil-cpp': {
    'url': Var('chromium_git') + '/chromium/src/third_party/abseil-cpp@7bb12f653a5c244263530ab61ae6850bf55a7d92',
    'condition': 'not build_with_chromium',
  },

  'third_party/android_build_tools': {
    'url': Var('chromium_git') + '/chromium/src/third_party/android_build_tools@b0d14299424646fc5247b863d543d33d67066800',
    'condition': 'checkout_android and not build_with_chromium',
  },

  'third_party/android_build_tools/aapt2/cipd': {
      'packages': [
          {
              'package': 'chromium/third_party/android_build_tools/aapt2',
              'version': 's6POXpUalcnuPehDsORiojCpgbNXT4LYq7DVUYgsfxEC',
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
               'version': 'sZ24OFOduSafn8fvR3ajsGS6KP_oS_Tq0Cw3SA8XiD0C',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_build_tools/error_prone/cipd': {
      'packages': [
          {
               'package': 'chromium/third_party/android_build_tools/error_prone',
               'version': '2NlHzICEPBy3pqjkWdRLE6wDGcrEK9_wmgIEA2Ctbv8C',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_build_tools/error_prone_javac/cipd': {
      'packages': [
          {
               'package': 'chromium/third_party/android_build_tools/error_prone_javac',
               'version': '7EcHxlEXEaLRWEyHIAxf0ouPjkmN1Od6jkutuo0sfBIC',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_build_tools/lint/cipd': {
      'packages': [
          {
               'package': 'chromium/third_party/android_build_tools/lint',
               'version': 'fzZu__O6lHUNn15lTLrpuGmA_KaVCmAXUOsafEWhQh8C',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_build_tools/manifest_merger/cipd': {
      'packages': [
          {
               'package': 'chromium/third_party/android_build_tools/manifest_merger',
               'version': 'GDEO78EYul-SF_vcCbzj7VLv-GDLWETnr5Ep7kDPs20C',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_build_tools/nullaway/cipd': {
      'packages': [
          {
               'package': 'chromium/third_party/android_build_tools/nullaway',
               'version': 'dhMJZGrjo_SN_MAYcQkEz81N-RrWqhM1pi5967oH9kAC',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps': {
    'url': Var('chromium_git') + '/chromium/src/third_party/android_deps@0e7dbe44cb0818cdbc555bcde31e2e5b1e1cd0df',
    'condition': 'checkout_android and not build_with_chromium',
  },

  'third_party/android_toolchain/ndk': {
      'packages': [
            {
                'package': 'chromium/third_party/android_toolchain/android_toolchain',
                'version': 'KXOia11cm9lVdUdPlbGLu8sCz6Y4ey_HV2s8_8qeqhgC',
            },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_platform': {
    'url': Var('chromium_git') + '/chromium/src/third_party/android_platform@e3919359f2387399042d31401817db4a02d756ec',
    'condition': 'checkout_android and not build_with_chromium',
  },

  'third_party/android_sdk': {
    'url': Var('chromium_git') + '/chromium/src/third_party/android_sdk@4dc44574065d12f8f5da15c2dd7ebf4c02eaf08a',
    'condition': 'checkout_android and not build_with_chromium',
  },

  'third_party/android_sdk/public': {
      'packages': [
          {
              'package': 'chromium/third_party/android_sdk/public/build-tools/36.0.0',
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
              'package': 'chromium/third_party/android_sdk/public/platforms/android-36',
              'version': Var('android_sdk_platforms_version'),
          },
          {
              'package': 'chromium/third_party/android_sdk/public/cmdline-tools',
              'version': 'gekOVsZjseS1w9BXAT3FsoW__ByGDJYS9DgqesiwKYoC',
          },
      ],
      'condition': 'checkout_android_native_support and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_system_sdk/cipd': {
      'packages': [
          {
              'package': 'chromium/third_party/android_system_sdk/public',
              'version': 'Pfb3HDUW_uRir_VVTCYkGhf6bnPPF55NUJO2WXOxIe0C',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium and non_git_source',
      'dep_type': 'cipd',
  },

  'third_party/astc-encoder/src': {
    'url': Var('chromium_git') + '/external/github.com/ARM-software/astc-encoder@2319d9c4d4af53a7fc7c52985e264ce6e8a02a9b',
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
    'url': Var('chromium_git') + '/external/github.com/google/clspv@8f35aa835ae973e1c802421ba5a873f5e967278c',
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
    'url': Var('chromium_git') + '/chromium/tools/depot_tools.git@8efa575d754b8703d99b0f827528e45aeaa167aa',
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
    'url': Var('chromium_git') + '/external/github.com/google/flatbuffers.git@187240970746d00bbd26b0f5873ed54d2477f9f3',
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
    'url': Var('chromium_git') + '/external/github.com/glmark2/glmark2@6edcf02205fd1e8979dc3f3964257a81959b80c8',
  },

  'third_party/googletest/src': {
    'url': Var('chromium_git') + '/external/github.com/google/googletest.git' + '@' + Var('googletest_revision'),
    'condition': 'not build_with_chromium',
  },

  'third_party/ijar': {
    'url': Var('chromium_git') + '/chromium/src/third_party/ijar@94af60a05b33f9acb33477a8d969e48eb1c3029f',
    'condition': 'checkout_android and not build_with_chromium',
  },

  'third_party/kotlin_stdlib': {
      'packages': [
          {
              'package': 'chromium/third_party/kotlin_stdlib',
              'version': 'LPDM6HLFsJuO_Crq0eaKCdr056AgYZ7Jz_D8NYTxragC',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/libdrm/src': {
    'url': Var('chromium_git') + '/chromiumos/third_party/libdrm.git@ad78bb591d02162d3b90890aa4d0a238b2a37cde',
    'condition': 'checkout_angle_mesa or not build_with_chromium',
  },

  # libjpeg_turbo is used by glmark2.
  'third_party/libjpeg_turbo': {
    'url': Var('chromium_git') + '/chromium/deps/libjpeg_turbo.git@6383cf609c1f63c18af0f59b2738caa0c6c7e379',
    'condition': 'not build_with_chromium',
  },

  'third_party/libpng/src': {
    'url': Var('android_git') + '/platform/external/libpng@8cc222cd3e79fa5190f3aa039a03a4cbea6cfbe7',
    'condition': 'not build_with_chromium',
  },

  'third_party/llvm/src': {
    'url': Var('chromium_git') + '/external/github.com/llvm/llvm-project@67790d3fb06b184b7be9c4d3e36ab01f0ac4198e',
    'condition': 'not build_with_chromium',
  },

  'third_party/jdk/current': {
      'packages': [
          {
              'package': 'chromium/third_party/jdk/linux-amd64',
              'version': '2iiuF-nKDH3moTImx2op4WTRetbfhzKoZhH7Xo44zGsC',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium and non_git_source',
      'dep_type': 'cipd',
  },

  'third_party/jinja2': {
    'url': Var('chromium_git') + '/chromium/src/third_party/jinja2@c3027d884967773057bf74b957e3fea87e5df4d7',
    'condition': 'not build_with_chromium',
  },

  'third_party/jsoncpp': {
    'url': Var('chromium_git') + '/chromium/src/third_party/jsoncpp' + '@' + Var('chromium_jsoncpp_revision'),
    'condition': 'not build_with_chromium',
   },

  'third_party/libc++/src': {
    'url': Var('chromium_git') + '/external/github.com/llvm/llvm-project/libcxx.git@13cfd0942dfd76fc838d1ead5c3c75a540f4760c',
    'condition': 'not build_with_chromium',
  },

  'third_party/libc++abi/src': {
    'url': Var('chromium_git') + '/external/github.com/llvm/llvm-project/libcxxabi.git@83a852080747b9a362e8f9e361366b7a601f302c',
    'condition': 'not build_with_chromium',
  },

  'third_party/llvm-libc/src': {
    'url': Var('chromium_git') + '/external/github.com/llvm/llvm-project/libc.git@c959effd96e5f33412ff10a31a79444e98ff18dc',
    'condition': 'not build_with_chromium',
  },

  'third_party/libunwind/src': {
    'url': Var('chromium_git') + '/external/github.com/llvm/llvm-project/libunwind.git@7ff36f1e358ffb63e25f27a1b5e8e0c52e15c49c',
    'condition': 'not build_with_chromium',
  },

  'third_party/markupsafe': {
    'url': Var('chromium_git') + '/chromium/src/third_party/markupsafe@4256084ae14175d38a3ff7d739dca83ae49ccec6',
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
    'url': Var('chromium_git') + '/chromium/deps/nasm.git@af5eeeb054bebadfbb79c7bcd100a95e2ad4525f',
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
    'url': Var('chromium_git') + '/external/github.com/KhronosGroup/OpenCL-CTS@9fc0d23b4cfccd84be8927363a77107dc554de30',
    'condition': 'not build_with_chromium',
  },

  'third_party/OpenCL-Docs/src': {
    'url': Var('chromium_git') + '/external/github.com/KhronosGroup/OpenCL-Docs@5b4ca15f0e5a5be87b56b99f652f728e05cab587',
    'condition': 'not build_with_chromium',
  },

  'third_party/OpenCL-ICD-Loader/src': {
    'url': Var('chromium_git') + '/external/github.com/KhronosGroup/OpenCL-ICD-Loader@ddf6c70230a79cdb8fcccfd3c775b09e6820f42e',
    'condition': 'not build_with_chromium',
  },

  'third_party/OpenGL-Registry/src': {
    'url': Var('chromium_git') + '/external/github.com/KhronosGroup/OpenGL-Registry@200cea4030cb49d3e40677379e6368a5f0e8c27b',
    'condition': 'not build_with_chromium',
  },

  'third_party/perfetto': {
    'url': Var('android_git') + '/platform/external/perfetto.git@5c17fc6e089cecec6bd75073875f57c99dcd2f02',
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
    'url': Var('chromium_git') + '/chromium/src/third_party/protobuf@2caa6ae88fd4eca3fb7e7e975fc9d841ca42defa',
    'condition': 'not build_with_chromium',
  },

  'third_party/Python-Markdown': {
    'url': Var('chromium_git') + '/chromium/src/third_party/Python-Markdown@0f4473546172a64636f5d841410c564c0edad625',
    'condition': 'not build_with_chromium',
  },

  'third_party/r8/cipd': {
      'packages': [
          {
              'package': 'chromium/third_party/r8',
              'version': 'sjuUu6_BDMGSci_jpKyt68J7gQ3sA59eJI7biFlTGXoC',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  # This duplication is intentional, so we avoid updating the r8.jar used by
  # dexing unless necessary, since each update invalidates all incremental
  # dexing and unnecessarily slows down all bots.
  'third_party/r8/d8/cipd': {
      'packages': [
          {
              'package': 'chromium/third_party/r8',
              'version': 'a4fVqbIycCDqs1714SLRqxEdz6P-sH-z1QT_eeeF0PcC',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/rapidjson/src': {
    'url': Var('chromium_git') + '/external/github.com/Tencent/rapidjson@781a4e667d84aeedbeb8184b7b62425ea66ec59f',
  },

  'third_party/re2/src': {
    'url': Var('chromium_git') + '/external/github.com/google/re2.git@e7aec5985072c1dbe735add802653ef4b36c231a',
    'condition': 'not build_with_chromium',
  },

  'third_party/requests/src': {
    'url': Var('chromium_git') + '/external/github.com/kennethreitz/requests.git@c7e0fc087ceeadb8b4c84a0953a422c474093d6d',
    'condition': 'checkout_android and not build_with_chromium',
  },

  'third_party/rust': {
    'url': Var('chromium_git') + '/chromium/src/third_party/rust@e6ee35e1c94c62365c41db142d17bb96210b6dad',
    'condition': 'not build_with_chromium',
  },

  'third_party/siso/cipd': {
    'packages': [
      {
        'package': 'build/siso/${{platform}}',
        'version': Var('siso_version'),
      }
    ],
    'dep_type': 'cipd',
    'condition': 'not build_with_chromium',
  },

  'third_party/six': {
    'url': Var('chromium_git') + '/chromium/src/third_party/six@98dedb5909b3e39848c6de5122772f5a89abe61a',
    'condition': 'checkout_android and not build_with_chromium',
  },

  'third_party/SwiftShader': {
    'url': Var('swiftshader_git') + '/SwiftShader@498a6f760dea6cdda0302406c49b8fbb09b9af92',
    'condition': 'not build_with_chromium',
  },

  'third_party/turbine/cipd': {
      'packages': [
          {
              'package': 'chromium/third_party/turbine',
              'version': 'Gl4akWHMtp73SMuAV-JWfB1w5zVhqYOmq61wo8TDChsC',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/VK-GL-CTS/src': {
    'url': Var('chromium_git') + '/external/github.com/KhronosGroup/VK-GL-CTS' + '@' + Var('vk_gl_cts_revision'),
  },

  'third_party/vulkan-deps': {
    'url': Var('chromium_git') + '/vulkan-deps@d1eb1a06c1ed64c9ab44fea1a3073fe74b6046ab',
    'condition': 'not build_with_chromium',
  },

  'third_party/glslang/src': {
    'url': '{chromium_git}/external/github.com/KhronosGroup/glslang@7a47e2531cb334982b2a2dd8513dca0a3de4373d',
    'condition': 'not build_with_chromium',
  },

  'third_party/lunarg-vulkantools/src': {
    'url': '{chromium_git}/external/github.com/LunarG/VulkanTools@d554b6eb885b8b4ebadbad08dec684900e840479',
    'condition': 'not build_with_chromium',
  },

  'third_party/spirv-cross/src': {
    'url': '{chromium_git}/external/github.com/KhronosGroup/SPIRV-Cross@b8fcf307f1f347089e3c46eb4451d27f32ebc8d3',
    'condition': 'not build_with_chromium',
  },

  'third_party/spirv-headers/src': {
    'url': '{chromium_git}/external/github.com/KhronosGroup/SPIRV-Headers@b824a462d4256d720bebb40e78b9eb8f78bbb305',
    'condition': 'not build_with_chromium',
  },

  'third_party/spirv-tools/src': {
    'url': '{chromium_git}/external/github.com/KhronosGroup/SPIRV-Tools@8a67272ca6c266b21dd0a9548471756a237ebbef',
    'condition': 'not build_with_chromium',
  },

  'third_party/vulkan-headers/src': {
    'url': '{chromium_git}/external/github.com/KhronosGroup/Vulkan-Headers@2fa203425eb4af9dfc6b03f97ef72b0b5bcb8350',
    'condition': 'not build_with_chromium',
  },

  'third_party/vulkan-loader/src': {
    'url': '{chromium_git}/external/github.com/KhronosGroup/Vulkan-Loader@052ac24611eced7b0ca62cc5cca2eeeb2051fa28',
    'condition': 'not build_with_chromium',
  },

  'third_party/vulkan-tools/src': {
    'url': '{chromium_git}/external/github.com/KhronosGroup/Vulkan-Tools@2a3347d5e74d359e3ecb8e229917f3335bfa2dfa',
    'condition': 'not build_with_chromium',
  },

  'third_party/vulkan-utility-libraries/src': {
    'url': '{chromium_git}/external/github.com/KhronosGroup/Vulkan-Utility-Libraries@c010c19e796035e92fb3b0462cb887518a41a7c1',
    'condition': 'not build_with_chromium',
  },

  'third_party/vulkan-validation-layers/src': {
    'url': '{chromium_git}/external/github.com/KhronosGroup/Vulkan-ValidationLayers@093b34b0801da9759645e6f179f89038d0c1e721',
    'condition': 'not build_with_chromium',
  },

  'third_party/vulkan_memory_allocator': {
    'url': Var('chromium_git') + '/external/github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator@cb0597213b0fcb999caa9ed08c2f88dc45eb7d50',
    'condition': 'not build_with_chromium',
  },

  'third_party/wayland': {
    'url': Var('chromium_git') + '/external/anongit.freedesktop.org/git/wayland/wayland@75c1a93e2067220fa06208f20f8f096bb463ec08',
    'condition': 'not build_with_chromium and host_os == "linux"'
  },

  'third_party/zlib': {
    'url': Var('chromium_git') + '/chromium/src/third_party/zlib@63d7e16739d83e3a16c673692a348e52db1a3a11',
    'condition': 'not build_with_chromium',
  },

  'tools/android': {
    'url': Var('chromium_git') + '/chromium/src/tools/android@a1ef7138e4e1b8e72b84aec5c727128880e9dd33',
    'condition': 'checkout_android and not build_with_chromium',
  },

  'tools/clang': {
    'url': Var('chromium_git') + '/chromium/src/tools/clang.git@3ff3017c03ef318a800230abf787e1e5f198d306',
    'condition': 'not build_with_chromium',
  },

  'tools/luci-go': {
    'packages': [
      {
        'package': 'infra/tools/luci/cas/${{platform}}',
        'version': Var('luci_go'),
      },
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
    'url': Var('chromium_git') + '/chromium/src/tools/mb@c1ee7e12fcad7ddb19e86ad72c38f6aed0fbc7b2',
    'condition': 'not build_with_chromium',
  },

  'tools/md_browser': {
    'url': Var('chromium_git') + '/chromium/src/tools/md_browser@b7cfebc8143108734248df4e855a1bff01173f77',
    'condition': 'not build_with_chromium',
  },

  'tools/memory': {
    'url': Var('chromium_git') + '/chromium/src/tools/memory@299412a3264f857e795082bd8c9aa3eb1d706e38',
    'condition': 'not build_with_chromium',
  },

  'tools/perf': {
    'url': Var('chromium_git') + '/chromium/src/tools/perf@7664d7ccf5cd2e3405e1d130243f693d0b212ebd',
    'condition': 'not build_with_chromium',
  },

  'tools/protoc_wrapper': {
    'url': Var('chromium_git') + '/chromium/src/tools/protoc_wrapper@3438d4183bfc7c0d6850e8b970204cc8189f0323',
    'condition': 'not build_with_chromium',
  },

  'tools/python': {
    'url': Var('chromium_git') + '/chromium/src/tools/python@64dd0e593f8e438764ced983a9f3f96061df748c',
    'condition': 'checkout_android and not build_with_chromium',
  },

  'tools/rust': {
    'url': Var('chromium_git') + '/chromium/src/tools/rust.git@ca091d9562b30d94c08154ebb0a3874ca40d4064',
    'condition': 'not build_with_chromium',
  },

  'tools/skia_goldctl/linux': {
      'packages': [
        {
          'package': 'skia/tools/goldctl/linux-amd64',
          'version': '8BaGVdmf90Ps7gF4-pDncJ-jiSRta5qxSzFu1qNuppwC',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_linux and not build_with_chromium',
  },

  'tools/skia_goldctl/win': {
      'packages': [
        {
          'package': 'skia/tools/goldctl/windows-amd64',
          'version': 'nSaaRu-IJVLSNJwmlj5lLGePsR7jhd8uUGAMDpNgBVsC',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_win and not build_with_chromium',
  },

  'tools/skia_goldctl/mac_amd64': {
      'packages': [
        {
          'package': 'skia/tools/goldctl/mac-amd64',
          'version': 'nPA_611Q7RCdi4a2t4ABXKPqGVpeW2PypITnNTPr9pEC',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_mac and not build_with_chromium',
  },

  'tools/skia_goldctl/mac_arm64': {
      'packages': [
        {
          'package': 'skia/tools/goldctl/mac-arm64',
          'version': 'PZOut7PwJPGaOA70L9rGgh-KKENEYQRFDnNl7N_qHhAC',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_mac and not build_with_chromium',
  },

  'tools/valgrind': {
    'url': Var('chromium_git') + '/chromium/src/tools/valgrind@da34b95fdbf2032df6cda5f3828c2ba421592644',
    'condition': 'not build_with_chromium',
  },

  'tools/win': {
    'url': Var('chromium_git') + '/chromium/src/tools/win@24494b071e019a2baea4355d9870ffc5fc0bbafe',
    'condition': 'checkout_win and not build_with_chromium',
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
  'src/tests/restricted_traces/ace_racer': {
      'packages': [
        {
            'package': 'angle/traces/ace_racer',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_ace_racer',
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
  'src/tests/restricted_traces/antistress': {
      'packages': [
        {
            'package': 'angle/traces/antistress',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_antistress',
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
  'src/tests/restricted_traces/apex_girl': {
      'packages': [
        {
            'package': 'angle/traces/apex_girl',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_apex_girl',
  },
  'src/tests/restricted_traces/archero_2': {
      'packages': [
        {
            'package': 'angle/traces/archero_2',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_archero_2',
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
  'src/tests/restricted_traces/avatar_world': {
      'packages': [
        {
            'package': 'angle/traces/avatar_world',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_avatar_world',
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
  'src/tests/restricted_traces/balatro': {
      'packages': [
        {
            'package': 'angle/traces/balatro',
            'version': 'version:2',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_balatro',
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
  'src/tests/restricted_traces/billiards_city': {
      'packages': [
        {
            'package': 'angle/traces/billiards_city',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_billiards_city',
  },
  'src/tests/restricted_traces/bingo_blitz': {
      'packages': [
        {
            'package': 'angle/traces/bingo_blitz',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_bingo_blitz',
  },
  'src/tests/restricted_traces/black_beacon': {
      'packages': [
        {
            'package': 'angle/traces/black_beacon',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_black_beacon',
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
  'src/tests/restricted_traces/block_blast': {
      'packages': [
        {
            'package': 'angle/traces/block_blast',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_block_blast',
  },
  'src/tests/restricted_traces/block_craft_3d': {
      'packages': [
        {
            'package': 'angle/traces/block_craft_3d',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_block_craft_3d',
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
  'src/tests/restricted_traces/brain_test_tricky_puzzles': {
      'packages': [
        {
            'package': 'angle/traces/brain_test_tricky_puzzles',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_brain_test_tricky_puzzles',
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
  'src/tests/restricted_traces/brawlhalla': {
      'packages': [
        {
            'package': 'angle/traces/brawlhalla',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_brawlhalla',
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
  'src/tests/restricted_traces/brotato': {
      'packages': [
        {
            'package': 'angle/traces/brotato',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_brotato',
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
  'src/tests/restricted_traces/bubble_shooter_relaxing': {
      'packages': [
        {
            'package': 'angle/traces/bubble_shooter_relaxing',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_bubble_shooter_relaxing',
  },
  'src/tests/restricted_traces/bullet_echo': {
      'packages': [
        {
            'package': 'angle/traces/bullet_echo',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_bullet_echo',
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
  'src/tests/restricted_traces/car_race_3d': {
      'packages': [
        {
            'package': 'angle/traces/car_race_3d',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_car_race_3d',
  },
  'src/tests/restricted_traces/carrom_pool_disc_game': {
      'packages': [
        {
            'package': 'angle/traces/carrom_pool_disc_game',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_carrom_pool_disc_game',
  },
  'src/tests/restricted_traces/cash_frenzy_casino_slots': {
      'packages': [
        {
            'package': 'angle/traces/cash_frenzy_casino_slots',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_cash_frenzy_casino_slots',
  },
  'src/tests/restricted_traces/cashman_casino': {
      'packages': [
        {
            'package': 'angle/traces/cashman_casino',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_cashman_casino',
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
  'src/tests/restricted_traces/cat_runner': {
      'packages': [
        {
            'package': 'angle/traces/cat_runner',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_cat_runner',
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
  'src/tests/restricted_traces/chrono_trigger': {
      'packages': [
        {
            'package': 'angle/traces/chrono_trigger',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_chrono_trigger',
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
            'version': 'version:6',
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
  'src/tests/restricted_traces/color_block_combo_blast': {
      'packages': [
        {
            'package': 'angle/traces/color_block_combo_blast',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_color_block_combo_blast',
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
            'version': 'version:6',
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
  'src/tests/restricted_traces/critical_ops': {
      'packages': [
        {
            'package': 'angle/traces/critical_ops',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_critical_ops',
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
  'src/tests/restricted_traces/dark_war_survival': {
      'packages': [
        {
            'package': 'angle/traces/dark_war_survival',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_dark_war_survival',
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
  'src/tests/restricted_traces/dc_dark_legion': {
      'packages': [
        {
            'package': 'angle/traces/dc_dark_legion',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_dc_dark_legion',
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
  'src/tests/restricted_traces/dice_dreams': {
      'packages': [
        {
            'package': 'angle/traces/dice_dreams',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_dice_dreams',
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
  'src/tests/restricted_traces/disney_solitaire': {
      'packages': [
        {
            'package': 'angle/traces/disney_solitaire',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_disney_solitaire',
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
  'src/tests/restricted_traces/domino_dreams': {
      'packages': [
        {
            'package': 'angle/traces/domino_dreams',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_domino_dreams',
  },
  'src/tests/restricted_traces/dont_starve': {
      'packages': [
        {
            'package': 'angle/traces/dont_starve',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_dont_starve',
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
  'src/tests/restricted_traces/downwell': {
      'packages': [
        {
            'package': 'angle/traces/downwell',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_downwell',
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
  'src/tests/restricted_traces/fashion_show': {
      'packages': [
        {
            'package': 'angle/traces/fashion_show',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_fashion_show',
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
            'version': 'version:6',
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
  'src/tests/restricted_traces/football_league_2025': {
      'packages': [
        {
            'package': 'angle/traces/football_league_2025',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_football_league_2025',
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
  'src/tests/restricted_traces/frost_and_flame': {
      'packages': [
        {
            'package': 'angle/traces/frost_and_flame',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_frost_and_flame',
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
            'version': 'version:6',
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
  'src/tests/restricted_traces/going_balls': {
      'packages': [
        {
            'package': 'angle/traces/going_balls',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_going_balls',
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
  'src/tests/restricted_traces/gossip_harbor': {
      'packages': [
        {
            'package': 'angle/traces/gossip_harbor',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_gossip_harbor',
  },
  'src/tests/restricted_traces/grand_mountain_adventure': {
      'packages': [
        {
            'package': 'angle/traces/grand_mountain_adventure',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_grand_mountain_adventure',
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
  'src/tests/restricted_traces/gwent_witcher_card_game': {
      'packages': [
        {
            'package': 'angle/traces/gwent_witcher_card_game',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_gwent_witcher_card_game',
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
  'src/tests/restricted_traces/harry_potter_magic_awakened': {
      'packages': [
        {
            'package': 'angle/traces/harry_potter_magic_awakened',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_harry_potter_magic_awakened',
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
  'src/tests/restricted_traces/hero_wars': {
      'packages': [
        {
            'package': 'angle/traces/hero_wars',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_hero_wars',
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
  'src/tests/restricted_traces/hole_io': {
      'packages': [
        {
            'package': 'angle/traces/hole_io',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_hole_io',
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
  'src/tests/restricted_traces/honkai_impact_3rd': {
      'packages': [
        {
            'package': 'angle/traces/honkai_impact_3rd',
            'version': 'version:2',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_honkai_impact_3rd',
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
  'src/tests/restricted_traces/hunter_assassin': {
      'packages': [
        {
            'package': 'angle/traces/hunter_assassin',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_hunter_assassin',
  },
  'src/tests/restricted_traces/i9_inferno_nine': {
      'packages': [
        {
            'package': 'angle/traces/i9_inferno_nine',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_i9_inferno_nine',
  },
  'src/tests/restricted_traces/identity_v': {
      'packages': [
        {
            'package': 'angle/traces/identity_v',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_identity_v',
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
  'src/tests/restricted_traces/indian_bikes_driving_3d': {
      'packages': [
        {
            'package': 'angle/traces/indian_bikes_driving_3d',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_indian_bikes_driving_3d',
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
  'src/tests/restricted_traces/jackpot_party': {
      'packages': [
        {
            'package': 'angle/traces/jackpot_party',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_jackpot_party',
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
  'src/tests/restricted_traces/kingshot': {
      'packages': [
        {
            'package': 'angle/traces/kingshot',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_kingshot',
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
  'src/tests/restricted_traces/knives_out': {
      'packages': [
        {
            'package': 'angle/traces/knives_out',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_knives_out',
  },
  'src/tests/restricted_traces/lands_of_jail': {
      'packages': [
        {
            'package': 'angle/traces/lands_of_jail',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_lands_of_jail',
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
  'src/tests/restricted_traces/last_war_survival_game': {
      'packages': [
        {
            'package': 'angle/traces/last_war_survival_game',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_last_war_survival_game',
  },
  'src/tests/restricted_traces/last_z_survival_shooter': {
      'packages': [
        {
            'package': 'angle/traces/last_z_survival_shooter',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_last_z_survival_shooter',
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
  'src/tests/restricted_traces/lightning_link': {
      'packages': [
        {
            'package': 'angle/traces/lightning_link',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_lightning_link',
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
  'src/tests/restricted_traces/lineage_2m': {
      'packages': [
        {
            'package': 'angle/traces/lineage_2m',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_lineage_2m',
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
  'src/tests/restricted_traces/loop_hero': {
      'packages': [
        {
            'package': 'angle/traces/loop_hero',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_loop_hero',
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
  'src/tests/restricted_traces/love_and_deepspace': {
      'packages': [
        {
            'package': 'angle/traces/love_and_deepspace',
            'version': 'version:2',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_love_and_deepspace',
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
            'version': 'version:2',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_marvel_strike_force',
  },
  'src/tests/restricted_traces/match_factory': {
      'packages': [
        {
            'package': 'angle/traces/match_factory',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_match_factory',
  },
  'src/tests/restricted_traces/match_masters': {
      'packages': [
        {
            'package': 'angle/traces/match_masters',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_match_masters',
  },
  'src/tests/restricted_traces/matching_story': {
      'packages': [
        {
            'package': 'angle/traces/matching_story',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_matching_story',
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
  'src/tests/restricted_traces/merge_mansion': {
      'packages': [
        {
            'package': 'angle/traces/merge_mansion',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_merge_mansion',
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
  'src/tests/restricted_traces/minecraft_vibrant_visuals': {
      'packages': [
        {
            'package': 'angle/traces/minecraft_vibrant_visuals',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_minecraft_vibrant_visuals',
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
  'src/tests/restricted_traces/mini_games_calm_and_relax': {
      'packages': [
        {
            'package': 'angle/traces/mini_games_calm_and_relax',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_mini_games_calm_and_relax',
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
  'src/tests/restricted_traces/mob_control': {
      'packages': [
        {
            'package': 'angle/traces/mob_control',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_mob_control',
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
  'src/tests/restricted_traces/monopoly_go': {
      'packages': [
        {
            'package': 'angle/traces/monopoly_go',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_monopoly_go',
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
  'src/tests/restricted_traces/my_talking_angela': {
      'packages': [
        {
            'package': 'angle/traces/my_talking_angela',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_my_talking_angela',
  },
  'src/tests/restricted_traces/my_talking_angela_2': {
      'packages': [
        {
            'package': 'angle/traces/my_talking_angela_2',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_my_talking_angela_2',
  },
  'src/tests/restricted_traces/my_talking_tom': {
      'packages': [
        {
            'package': 'angle/traces/my_talking_tom',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_my_talking_tom',
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
  'src/tests/restricted_traces/need_for_speed_no_limits': {
      'packages': [
        {
            'package': 'angle/traces/need_for_speed_no_limits',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_need_for_speed_no_limits',
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
  'src/tests/restricted_traces/odin_valhalla_rising': {
      'packages': [
        {
            'package': 'angle/traces/odin_valhalla_rising',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_odin_valhalla_rising',
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
  'src/tests/restricted_traces/offline_games': {
      'packages': [
        {
            'package': 'angle/traces/offline_games',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_offline_games',
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
  'src/tests/restricted_traces/onmyoji': {
      'packages': [
        {
            'package': 'angle/traces/onmyoji',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_onmyoji',
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
  'src/tests/restricted_traces/paper_io2': {
      'packages': [
        {
            'package': 'angle/traces/paper_io2',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_paper_io2',
  },
  'src/tests/restricted_traces/passmark_complex': {
      'packages': [
        {
            'package': 'angle/traces/passmark_complex',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_passmark_complex',
  },
  'src/tests/restricted_traces/passmark_simple': {
      'packages': [
        {
            'package': 'angle/traces/passmark_simple',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_passmark_simple',
  },
  'src/tests/restricted_traces/passmark_unity': {
      'packages': [
        {
            'package': 'angle/traces/passmark_unity',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_passmark_unity',
  },
  'src/tests/restricted_traces/piano_fire': {
      'packages': [
        {
            'package': 'angle/traces/piano_fire',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_piano_fire',
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
            'version': 'version:6',
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
  'src/tests/restricted_traces/puzzle_and_dragons': {
      'packages': [
        {
            'package': 'angle/traces/puzzle_and_dragons',
            'version': 'version:2',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_puzzle_and_dragons',
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
  'src/tests/restricted_traces/qblock_wood_block_puzzle_game': {
      'packages': [
        {
            'package': 'angle/traces/qblock_wood_block_puzzle_game',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_qblock_wood_block_puzzle_game',
  },
  'src/tests/restricted_traces/race_master_3d': {
      'packages': [
        {
            'package': 'angle/traces/race_master_3d',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_race_master_3d',
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
  'src/tests/restricted_traces/raising_a_goblin': {
      'packages': [
        {
            'package': 'angle/traces/raising_a_goblin',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_raising_a_goblin',
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
  'src/tests/restricted_traces/return_to_monkey_island': {
      'packages': [
        {
            'package': 'angle/traces/return_to_monkey_island',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_return_to_monkey_island',
  },
  'src/tests/restricted_traces/riptide_gp2': {
      'packages': [
        {
            'package': 'angle/traces/riptide_gp2',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_riptide_gp2',
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
  'src/tests/restricted_traces/royal_kingdom': {
      'packages': [
        {
            'package': 'angle/traces/royal_kingdom',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_royal_kingdom',
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
  'src/tests/restricted_traces/school_party_craft': {
      'packages': [
        {
            'package': 'angle/traces/school_party_craft',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_school_party_craft',
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
  'src/tests/restricted_traces/seaside_escape': {
      'packages': [
        {
            'package': 'angle/traces/seaside_escape',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_seaside_escape',
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
            'version': 'version:2',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_shadowgun_legends',
  },
  'src/tests/restricted_traces/shovel_knight_pocket_dungeon': {
      'packages': [
        {
            'package': 'angle/traces/shovel_knight_pocket_dungeon',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_shovel_knight_pocket_dungeon',
  },
  'src/tests/restricted_traces/simcity_buildit': {
      'packages': [
        {
            'package': 'angle/traces/simcity_buildit',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_simcity_buildit',
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
  'src/tests/restricted_traces/slotomania': {
      'packages': [
        {
            'package': 'angle/traces/slotomania',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_slotomania',
  },
  'src/tests/restricted_traces/snake_io': {
      'packages': [
        {
            'package': 'angle/traces/snake_io',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_snake_io',
  },
  'src/tests/restricted_traces/snake_lite': {
      'packages': [
        {
            'package': 'angle/traces/snake_lite',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_snake_lite',
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
  'src/tests/restricted_traces/soccer_superstar': {
      'packages': [
        {
            'package': 'angle/traces/soccer_superstar',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_soccer_superstar',
  },
  'src/tests/restricted_traces/solar_smash': {
      'packages': [
        {
            'package': 'angle/traces/solar_smash',
            'version': 'version:3',
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
  'src/tests/restricted_traces/spider_fighter_3': {
      'packages': [
        {
            'package': 'angle/traces/spider_fighter_3',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_spider_fighter_3',
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
            'version': 'version:2',
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
  'src/tests/restricted_traces/super_bear_adventure': {
      'packages': [
        {
            'package': 'angle/traces/super_bear_adventure',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_super_bear_adventure',
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
  'src/tests/restricted_traces/supreme_duelist': {
      'packages': [
        {
            'package': 'angle/traces/supreme_duelist',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_supreme_duelist',
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
  'src/tests/restricted_traces/talking_tom_gold_run': {
      'packages': [
        {
            'package': 'angle/traces/talking_tom_gold_run',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_talking_tom_gold_run',
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
  'src/tests/restricted_traces/thief_puzzle': {
      'packages': [
        {
            'package': 'angle/traces/thief_puzzle',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_thief_puzzle',
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
  'src/tests/restricted_traces/tile_club': {
      'packages': [
        {
            'package': 'angle/traces/tile_club',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_tile_club',
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
            'version': 'version:3',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_toca_life_world',
  },
  'src/tests/restricted_traces/toon_blast': {
      'packages': [
        {
            'package': 'angle/traces/toon_blast',
            'version': 'version:6',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_toon_blast',
  },
  'src/tests/restricted_traces/top_heroes_kingdom_saga': {
      'packages': [
        {
            'package': 'angle/traces/top_heroes_kingdom_saga',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_top_heroes_kingdom_saga',
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
  'src/tests/restricted_traces/total_battle': {
      'packages': [
        {
            'package': 'angle/traces/total_battle',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_total_battle',
  },
  'src/tests/restricted_traces/tower_of_fantasy': {
      'packages': [
        {
            'package': 'angle/traces/tower_of_fantasy',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_tower_of_fantasy',
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
  'src/tests/restricted_traces/traffic_rider': {
      'packages': [
        {
            'package': 'angle/traces/traffic_rider',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_traffic_rider',
  },
  'src/tests/restricted_traces/travel_town': {
      'packages': [
        {
            'package': 'angle/traces/travel_town',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_travel_town',
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
  'src/tests/restricted_traces/two_player_games': {
      'packages': [
        {
            'package': 'angle/traces/two_player_games',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_two_player_games',
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
  'src/tests/restricted_traces/umamusume_pretty_derby': {
      'packages': [
        {
            'package': 'angle/traces/umamusume_pretty_derby',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_umamusume_pretty_derby',
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
  'src/tests/restricted_traces/vita_mahjong': {
      'packages': [
        {
            'package': 'angle/traces/vita_mahjong',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_vita_mahjong',
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
  'src/tests/restricted_traces/warhammer_40000_freeblade': {
      'packages': [
        {
            'package': 'angle/traces/warhammer_40000_freeblade',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_warhammer_40000_freeblade',
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
  'src/tests/restricted_traces/whiteout_survival': {
      'packages': [
        {
            'package': 'angle/traces/whiteout_survival',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_whiteout_survival',
  },
  'src/tests/restricted_traces/woodoku': {
      'packages': [
        {
            'package': 'angle/traces/woodoku',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_woodoku',
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
  'src/tests/restricted_traces/wuthering_waves': {
      'packages': [
        {
            'package': 'angle/traces/wuthering_waves',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_wuthering_waves',
  },
  'src/tests/restricted_traces/yalla_ludo': {
      'packages': [
        {
            'package': 'angle/traces/yalla_ludo',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_trace_yalla_ludo',
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
  'third_party/android_deps/cipd/libs/com_android_tools_common': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_tools_common',
              'version': 'version:2@30.2.0-beta01.cr2',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium and non_git_source',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/cipd/libs/com_android_tools_layoutlib_layoutlib_api': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_tools_layoutlib_layoutlib_api',
              'version': 'version:2@30.2.0-beta01.cr2',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium and non_git_source',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/cipd/libs/com_android_tools_sdk_common': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_tools_sdk_common',
              'version': 'version:2@30.2.0-beta01.cr2',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium and non_git_source',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/cipd/libs/com_google_android_apps_common_testing_accessibility_framework_accessibility_test_framework': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_android_apps_common_testing_accessibility_framework_accessibility_test_framework',
              'version': 'version:2@4.0.0.cr2',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium and non_git_source',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/cipd/libs/com_googlecode_java_diff_utils_diffutils': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_googlecode_java_diff_utils_diffutils',
              'version': 'version:2@1.3.0.cr2',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium and non_git_source',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/cipd/libs/com_squareup_javapoet': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_squareup_javapoet',
              'version': 'version:2@1.13.0.cr2',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium and non_git_source',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/cipd/libs/net_bytebuddy_byte_buddy': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/net_bytebuddy_byte_buddy',
              'version': 'version:2@1.17.6.cr2',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium and non_git_source',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/cipd/libs/net_bytebuddy_byte_buddy_agent': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/net_bytebuddy_byte_buddy_agent',
              'version': 'version:2@1.17.6.cr2',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium and non_git_source',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/cipd/libs/org_ccil_cowan_tagsoup_tagsoup': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_ccil_cowan_tagsoup_tagsoup',
              'version': 'version:2@1.2.1.cr2',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium and non_git_source',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/cipd/libs/org_checkerframework_checker_compat_qual': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_checkerframework_checker_compat_qual',
              'version': 'version:2@2.5.5.cr2',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium and non_git_source',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/cipd/libs/org_jetbrains_kotlin_kotlin_android_extensions_runtime': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_jetbrains_kotlin_kotlin_android_extensions_runtime',
              'version': 'version:2@1.9.22.cr2',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium and non_git_source',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/cipd/libs/org_jetbrains_kotlin_kotlin_parcelize_runtime': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_jetbrains_kotlin_kotlin_parcelize_runtime',
              'version': 'version:2@1.9.22.cr2',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium and non_git_source',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/cipd/libs/org_jetbrains_kotlinx_atomicfu_jvm': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_jetbrains_kotlinx_atomicfu_jvm',
              'version': 'version:2@0.23.2.cr2',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium and non_git_source',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/cipd/libs/org_jetbrains_kotlinx_kotlinx_coroutines_guava': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_jetbrains_kotlinx_kotlinx_coroutines_guava',
              'version': 'version:2@1.8.1.cr2',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium and non_git_source',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/cipd/libs/org_jsoup_jsoup': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_jsoup_jsoup',
              'version': 'version:2@1.15.1.cr2',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium and non_git_source',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/cipd/libs/org_mockito_mockito_android': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_mockito_mockito_android',
              'version': 'version:2@5.19.0.cr2',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium and non_git_source',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/cipd/libs/org_mockito_mockito_core': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_mockito_mockito_core',
              'version': 'version:2@5.19.0.cr2',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium and non_git_source',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/cipd/libs/org_mockito_mockito_subclass': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_mockito_mockito_subclass',
              'version': 'version:2@5.19.0.cr2',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium and non_git_source',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/cipd/libs/org_objenesis_objenesis': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_objenesis_objenesis',
              'version': 'version:2@3.3.cr2',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium and non_git_source',
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
    # Pull down one additional llvm dependency
    'name': 'llvm_objdump',
    'pattern': '.',
    'action': ['python3', 'tools/clang/scripts/update.py', '--package', 'objdump'],
    'condition': 'not build_with_chromium',
  },

  {
    'name': 'rust',
    'pattern': '.',
    'action': ['python3', 'tools/rust/update_rust.py'],
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

  # Pull dsymutil binaries using checked-in hashes.
  {
    'name': 'dsymutil_mac_arm64',
    'pattern': '.',
    'condition': 'host_os == "mac" and host_cpu == "arm64" and not build_with_chromium',
    'action': [ 'python3',
                'third_party/depot_tools/download_from_google_storage.py',
                '--no_resume',
                '--bucket', 'chromium-browser-clang',
                '-s', 'tools/clang/dsymutil/bin/dsymutil.arm64.sha1',
                '-o', 'tools/clang/dsymutil/bin/dsymutil',
    ],
  },
  {
    'name': 'dsymutil_mac_x64',
    'pattern': '.',
    'condition': 'host_os == "mac" and host_cpu == "x64" and not build_with_chromium',
    'action': [ 'python3',
                'third_party/depot_tools/download_from_google_storage.py',
                '--no_resume',
                '--bucket', 'chromium-browser-clang',
                '-s', 'tools/clang/dsymutil/bin/dsymutil.x64.sha1',
                '-o', 'tools/clang/dsymutil/bin/dsymutil',
    ],
  },
  # Pull rc binaries using checked-in hashes.
  {
    'name': 'rc_win',
    'pattern': '.',
    'condition': 'checkout_win and host_os == "win" and not build_with_chromium',
    'action': [ 'python3',
                'third_party/depot_tools/download_from_google_storage.py',
                '--no_resume',
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
                '--bucket', 'angle-flex-bison',
                '-d', 'tools/flex-bison/linux/',
    ],
  },

  # Download flex/bison binaries for Mac.
  {
    'name': 'mac_flex_bison',
    'pattern': '.',
    'condition': 'checkout_mac and not build_with_chromium',
    'action': [ 'python3',
                'third_party/depot_tools/download_from_google_storage.py',
                '--no_resume',
                '--platform=darwin*',
                '--bucket', 'angle-flex-bison',
                '-d', 'tools/flex-bison/mac/',
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
  'third_party/jsoncpp',
  'third_party/dawn',
]

skip_child_includes = [
    'third_party'
]
