/* Libvisual - The audio visualisation framework.
 *
 * Copyright (C) 2012      Libvisual team
 *               2004-2006 Dennis Smit
 *
 * Authors: Dennis Smit <ds@nerds-incorporated.org>
 *          Chong Kai Xiong <kaixiong@codeleft.sg>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include "config.h"
#include "lv_bin.h"
#include "lv_common.h"
#include "lv_list.h"

namespace LV {

  class Bin::Impl
  {
  public:

      VisActor* actor;
      VideoPtr  actvideo;
      VideoPtr  privvid;

      VideoPtr  actmorphvideo;
      VisActor* actmorph;

      VisInput* input;

      bool         use_morph;
      VisMorph*    morph;
      bool         morphing;
      VisMorphMode morphmode;
      Time         morphtime;

      VisBinDepth   depthpreferred;    /* Prefered depth, highest or lowest */
      VisVideoDepth depthflag;         /* Supported depths */
      VisVideoDepth depthold;          /* Previous depth */
      VisVideoDepth depth;             /* Depth we're running in */
      bool          depthchanged;      /* Set true if the depth has changed */
      bool          depthfromGL;       /* Set when switching away from openGL */
      VisVideoDepth depthforced;       /* Contains forced depth value, for the actmorph so we've got smooth transformations */
      VisVideoDepth depthforcedmain;   /* Contains forced depth value, for the main actor */

      Impl ();

      ~Impl ();

      VisVideoDepth get_suitable_depth (VisVideoDepth depth);

	  void set_actor (VisActor* actor);
	  void set_input (VisInput* input);
  };

  VisVideoDepth Bin::Impl::get_suitable_depth (VisVideoDepth depthflag)
  {
      VisVideoDepth depth = VISUAL_VIDEO_DEPTH_NONE;

      switch (depthpreferred) {
          case VISUAL_BIN_DEPTH_LOWEST:
              depth = visual_video_depth_get_lowest (depthflag);
              break;

          case VISUAL_BIN_DEPTH_HIGHEST:
              depth = visual_video_depth_get_highest (depthflag);
              break;
      }

      // Is supported within bin natively
      if (depthflag & depth)
          return depth;

      // Not supported by the bin, taking the highest depth from the bin
      return visual_video_depth_get_highest_nogl (depthflag);
  }


  Bin::Impl::Impl ()
      : actor           (nullptr)
      , actmorph        (nullptr)
      , input           (nullptr)
      , use_morph       (false)
      , morph           (nullptr)
      , morphing        (false)
      , morphtime       (4, 0)
      , depthpreferred  (VISUAL_BIN_DEPTH_HIGHEST)
      , depthflag       (VISUAL_VIDEO_DEPTH_NONE)
      , depthold        (VISUAL_VIDEO_DEPTH_NONE)
      , depth           (VISUAL_VIDEO_DEPTH_NONE)
      , depthchanged    (false)
      , depthfromGL     (false)
      , depthforced     (VISUAL_VIDEO_DEPTH_NONE)
      , depthforcedmain (VISUAL_VIDEO_DEPTH_NONE)
  {
      // empty
  }

  Bin::Impl::~Impl ()
  {
      if (actor)
          visual_object_unref (VISUAL_OBJECT (actor));

      if (input)
          visual_object_unref (VISUAL_OBJECT (input));

      if (morph)
          visual_object_unref (VISUAL_OBJECT (morph));

      if (actmorph)
          visual_object_unref (VISUAL_OBJECT (actmorph));
  }

  void Bin::Impl::set_actor (VisActor *new_actor)
  {
      if (actor) {
          visual_object_unref (VISUAL_OBJECT (actor));
      }

      actor = new_actor;

      if (actor) {
          visual_object_ref (VISUAL_OBJECT (actor));
      }
  }

  void Bin::Impl::set_input (VisInput* new_input)
  {
      if (input) {
          visual_object_unref (VISUAL_OBJECT (input));
      }

      input = new_input;

      if (input) {
          visual_object_ref (VISUAL_OBJECT (input));
      }
  }

  Bin::Bin ()
      : m_impl (new Impl)
  {
      // empty
  }

  Bin::~Bin ()
  {
      // empty
  }

  void Bin::realize ()
  {
      if (m_impl->actor)
          visual_actor_realize (m_impl->actor);

      if (m_impl->input)
          visual_input_realize (m_impl->input);

      if (m_impl->morph)
          visual_morph_realize (m_impl->morph);
  }

  VisActor* Bin::get_actor () const
  {
      return m_impl->actor;
  }

  VisInput *Bin::get_input () const
  {
      return m_impl->input;
  }

  void Bin::set_morph (std::string const& morphname)
  {
      if (m_impl->morph)
          visual_object_unref (VISUAL_OBJECT (m_impl->morph));

      VisMorph* morph = visual_morph_new (morphname.c_str ());

      m_impl->morph = morph;
      visual_return_if_fail (morph->plugin != nullptr);

      VisVideoDepth depthflag = visual_morph_get_supported_depth (morph);

      if (visual_video_depth_is_supported (depthflag, m_impl->actvideo->get_depth ()) <= 0) {
          visual_object_unref (VISUAL_OBJECT (morph));
          m_impl->morph = nullptr;

          return;
      }
  }

  VisMorph* Bin::get_morph () const
  {
      return m_impl->morph;
  }

  bool Bin::connect (VisActor *actor, VisInput *input)
  {
      visual_return_val_if_fail (actor != nullptr, false);
      visual_return_val_if_fail (input != nullptr, false);

      m_impl->set_actor (actor);
      m_impl->set_input (input);

      auto depthflag = visual_actor_get_supported_depths (actor);

      if (depthflag == VISUAL_VIDEO_DEPTH_GL) {
          set_depth (VISUAL_VIDEO_DEPTH_GL);
      } else {
          set_depth (m_impl->get_suitable_depth(depthflag));
      }

      m_impl->depthforcedmain = m_impl->depth;

      return true;
  }

  bool Bin::connect (std::string const& actname, std::string const& inname)
  {
      /* Create the actor */
      auto actor = visual_actor_new (actname.c_str ());
      visual_return_val_if_fail (actor != nullptr, false);

      /* Create the input */
      auto input = visual_input_new (inname.c_str ());
      visual_return_val_if_fail (input != nullptr, false);

      /* Connect */
      if (!connect (actor, input)) {
          return false;
      }

      return true;
  }

  void Bin::sync (bool noevent)
  {
      visual_log (VISUAL_LOG_DEBUG, "starting sync");

      VideoPtr video;

      /* Sync the actor regarding morph */
      if (m_impl->morphing && m_impl->use_morph &&
          m_impl->actvideo->get_depth () != VISUAL_VIDEO_DEPTH_GL && !m_impl->depthfromGL) {

          visual_morph_set_video (m_impl->morph, m_impl->actvideo.get ());

          video = m_impl->privvid;
          if (!video) {
              visual_log (VISUAL_LOG_DEBUG, "Private video data nullptr");
              return;
          }

          video->free_buffer ();
          video->copy_attrs (m_impl->actvideo);

          visual_log (VISUAL_LOG_DEBUG, "pitches actvideo %d, new video %d",
                      m_impl->actvideo->get_pitch (), video->get_pitch ());

          visual_log (VISUAL_LOG_DEBUG, "phase1 m_impl->privvid %p", (void *) m_impl->privvid.get ());
          if (visual_actor_get_video (m_impl->actmorph)->get_depth () == VISUAL_VIDEO_DEPTH_GL) {
              video = m_impl->actvideo;
          } else
              video->allocate_buffer ();

          visual_log (VISUAL_LOG_DEBUG, "phase2");
      } else {
          video = m_impl->actvideo;
          if (!video) {
              visual_log (VISUAL_LOG_DEBUG, "Actor video is nullptr");
              return;
          }

          visual_log (VISUAL_LOG_DEBUG, "setting new video from actvideo %d %d",
                      video->get_depth (), video->get_bpp ());
      }

      /* Main actor */
      /*    visual_actor_realize (m_impl->actor); */
      visual_actor_set_video (m_impl->actor, video.get ());

      visual_log (VISUAL_LOG_DEBUG, "one last video pitch check %d depth old %d forcedmain %d noevent %d",
                  video->get_pitch (), m_impl->depthold,
                  m_impl->depthforcedmain, noevent);

      if (m_impl->depthold == VISUAL_VIDEO_DEPTH_GL) {
          visual_actor_video_negotiate (m_impl->actor, m_impl->depthforcedmain, false, true);
      }
      else {
          visual_actor_video_negotiate (m_impl->actor, m_impl->depthforcedmain, noevent, true);
      }

      visual_log (VISUAL_LOG_DEBUG, "pitch after main actor negotiate %d", video->get_pitch ());

      /* Morphing actor */
      if (m_impl->morphing && m_impl->use_morph) {

          auto actvideo = m_impl->actmorphvideo;
          if (!actvideo) {
              visual_log (VISUAL_LOG_DEBUG, "Morph video is nullptr");
              return;
          }

          actvideo->free_buffer ();

          actvideo->copy_attrs (video);

          if (visual_actor_get_video (m_impl->actor)->get_depth () != VISUAL_VIDEO_DEPTH_GL)
              actvideo->allocate_buffer ();

          visual_actor_realize (m_impl->actmorph);

          visual_log (VISUAL_LOG_DEBUG, "phase3 pitch of real framebuffer %d",
                      m_impl->actvideo->get_pitch ());

          visual_actor_video_negotiate (m_impl->actmorph, m_impl->depthforced, false, true);
      }

      visual_log (VISUAL_LOG_DEBUG, "end sync function");
  }

  void Bin::set_video (VideoPtr const& video)
  {
      m_impl->actvideo = video;
  }

  void Bin::set_supported_depth (VisVideoDepth depthflag)
  {
      m_impl->depthflag = depthflag;
  }

  VisVideoDepth Bin::get_supported_depth () const
  {
      return m_impl->depthflag;
  }

  void Bin::set_preferred_depth (VisBinDepth depthpreferred)
  {
      m_impl->depthpreferred = depthpreferred;
  }

  void Bin::set_depth (VisVideoDepth depth)
  {
      m_impl->depthold = m_impl->depth;

      if (!visual_video_depth_is_supported (m_impl->depthflag, depth))
          return;

      visual_log (VISUAL_LOG_DEBUG, "old: %d new: %d", m_impl->depth, depth);

      if (m_impl->depth != depth)
          m_impl->depthchanged = true;

      if (m_impl->depth == VISUAL_VIDEO_DEPTH_GL && m_impl->depthchanged)
          m_impl->depthfromGL = true;
      else
          m_impl->depthfromGL = false;

      m_impl->depth = depth;

      if (m_impl->actvideo) {
          m_impl->actvideo->set_depth (depth);
      }
  }

  VisVideoDepth Bin::get_depth () const
  {
      return m_impl->depth;
  }

  bool Bin::depth_changed ()
  {
      if (!m_impl->depthchanged)
          return false;

      m_impl->depthchanged = false;

      return true;
  }

  Palette const& Bin::get_palette () const
  {
      if (m_impl->morphing)
          return *visual_morph_get_palette (m_impl->morph);
      else
          return *visual_actor_get_palette (m_impl->actor);
  }

  void Bin::switch_actor (std::string const& actname)
  {
      visual_log (VISUAL_LOG_DEBUG, "switching to a new actor: %s, old actor: %s",
				  actname.c_str (), visual_actor_get_plugin (m_impl->actor)->info->plugname);

      if (m_impl->actmorph) {
          visual_object_unref (VISUAL_OBJECT (m_impl->actmorph));
          m_impl->actmorphvideo = nullptr;
      }

      /* Create a new managed actor */
      auto actor = visual_actor_new (actname.c_str ());
      visual_return_if_fail (actor != nullptr);

      auto video = LV::Video::create();
      video->copy_attrs(m_impl->actvideo);

      auto depthflag = visual_actor_get_supported_depths (actor);
      VisVideoDepth depth;

      if (visual_video_depth_is_supported (depthflag, VISUAL_VIDEO_DEPTH_GL)) {
          visual_log (VISUAL_LOG_INFO, "Switching to GL mode");

          depth = VISUAL_VIDEO_DEPTH_GL;

          m_impl->depthforced = depth;
          m_impl->depthforcedmain = depth;

          video->set_depth(depth);

          set_depth (depth);

          m_impl->depthchanged = true;
      } else {
          visual_log (VISUAL_LOG_INFO, "Switching away from Gl mode -- or non Gl switch");

          /* Switching from GL */
          depth = m_impl->get_suitable_depth (depthflag);
          video->set_depth(depth);

          visual_log (VISUAL_LOG_DEBUG, "after depth fixating");

          /* After a depth change, the pitch value needs an update from the client
           * if it's different from width * bpp, after a visual_bin_sync
           * the issues are fixed again */
          visual_log (VISUAL_LOG_INFO, "video depth (from fixate): %d", video->get_depth ());

          /* FIXME check if there are any unneeded depth transform environments and drop these */
          visual_log (VISUAL_LOG_DEBUG, "checking if we need to drop something: depthforcedmain: %d actvideo->depth %d",
                      m_impl->depthforcedmain, m_impl->actvideo->get_depth ());

          /* Drop a transformation environment when not needed */
          if (m_impl->depthforcedmain != m_impl->actvideo->get_depth ()) {
              visual_actor_video_negotiate (m_impl->actor, m_impl->depthforcedmain, true, true);
              visual_log (VISUAL_LOG_DEBUG, "[[[[optionally a bogus transform environment, dropping]]]]");
          }

          if (m_impl->actvideo->get_depth () > video->get_depth ()
              && m_impl->actvideo->get_depth () != VISUAL_VIDEO_DEPTH_GL
              && m_impl->use_morph) {

              visual_log (VISUAL_LOG_INFO, "old depth is higher, video depth %d, bin depth %d",
                          video->get_depth (), m_impl->depth);

              m_impl->depthforced = depth;;
              m_impl->depthforcedmain = m_impl->depth;

              set_depth (m_impl->actvideo->get_depth ());
              video->set_depth (m_impl->actvideo->get_depth ());

          } else if (m_impl->actvideo->get_depth () != VISUAL_VIDEO_DEPTH_GL) {

              visual_log (VISUAL_LOG_INFO, "new depth is higher, or equal: video depth %d, depth %d bin depth %d",
                          video->get_depth (), depth, m_impl->depth);

              visual_log (VISUAL_LOG_DEBUG, "depths i can locate: actvideo: %d   bin: %d     bin-old: %d",
                          m_impl->actvideo->get_depth (), m_impl->depth, m_impl->depthold);

              m_impl->depthforced = video->get_depth ();
              m_impl->depthforcedmain = m_impl->depth;

              visual_log (VISUAL_LOG_DEBUG, "depthforcedmain in switch by name: %d", m_impl->depthforcedmain);
              visual_log (VISUAL_LOG_DEBUG, "Bin::set_depth %d", video->get_depth ());
              set_depth (video->get_depth ());

          } else {
              /* Don't force ourself into a GL depth, seen we do a direct
               * switch in the run */
              m_impl->depthforced = video->get_depth ();
              m_impl->depthforcedmain = video->get_depth ();

              visual_log (VISUAL_LOG_INFO, "Switching from GL to framebuffer for real, framebuffer depth: %d",
                          video->get_depth ());
          }

          visual_log (VISUAL_LOG_INFO, "Target depth selected: %d", depth);

          video->set_pitch(video->get_width() * visual_video_bpp_from_depth(depth));

          video->allocate_buffer();
      }


      visual_log (VISUAL_LOG_INFO, "video pitch of that what connects to the new actor %d",
                  video->get_pitch ());

      visual_actor_set_video (actor, video.get ());

      m_impl->actmorphvideo = video;

      visual_log (VISUAL_LOG_INFO, "switching... ******************************************");
      switch_actor (actor);

      visual_log (VISUAL_LOG_INFO, "end switch actor by name function ******************");
  }

  void Bin::switch_actor (VisActor *actor)
  {
      visual_return_if_fail (actor != nullptr);

      /* Set the new actor */
      m_impl->actmorph = actor;

      visual_log (VISUAL_LOG_DEBUG, "Starting actor switch...");

      /* Free the private video */
      m_impl->privvid = nullptr;

      visual_log (VISUAL_LOG_INFO, "depth of the main actor: %d",
                  visual_actor_get_video (m_impl->actor)->get_depth ());

      /* Starting the morph, but first check if we don't have anything todo with openGL */
      if (m_impl->use_morph &&
          visual_actor_get_video (m_impl->actor)->get_depth () != VISUAL_VIDEO_DEPTH_GL &&
          visual_actor_get_video (m_impl->actmorph)->get_depth () != VISUAL_VIDEO_DEPTH_GL &&
          !m_impl->depthfromGL) {

          if (m_impl->morph && m_impl->morph->plugin) {
              visual_morph_set_progress (m_impl->morph, 0.0);
              visual_morph_set_video (m_impl->morph, m_impl->actvideo.get ());
              visual_morph_set_mode (m_impl->morph, VISUAL_MORPH_MODE_TIME);
              visual_morph_set_time (m_impl->morph, &m_impl->morphtime);
          }

          visual_log (VISUAL_LOG_DEBUG, "phase 1");
          /* Allocate a private video for the main actor, so the morph
           * can draw to the framebuffer */
          auto privvid = Video::create ();

          visual_log (VISUAL_LOG_DEBUG, "actvideo->depth %d actmorph->video->depth %d",
                      m_impl->actvideo->get_depth (),
                      visual_actor_get_video (m_impl->actmorph)->get_depth ());

          visual_log (VISUAL_LOG_DEBUG, "phase 2");
          privvid->copy_attrs (m_impl->actvideo);

          visual_log (VISUAL_LOG_DEBUG, "phase 3 pitch privvid %d actvideo %d",
                      privvid->get_pitch (), m_impl->actvideo->get_pitch ());

          privvid->allocate_buffer ();

          visual_log (VISUAL_LOG_DEBUG, "phase 4");
          /* Initial privvid initialize */

          visual_log (VISUAL_LOG_DEBUG, "actmorph->video->depth %d %p",
                      visual_actor_get_video (m_impl->actmorph)->get_depth (),
                      m_impl->actvideo->get_pixels ());

          if (m_impl->actvideo->get_pixels () && privvid->get_pixels ())
              visual_mem_copy (privvid->get_pixels (), m_impl->actvideo->get_pixels (),
                               privvid->get_size ());
          else if (privvid->get_pixels ())
              visual_mem_set (privvid->get_pixels (), 0, privvid->get_size ());

          visual_actor_set_video (m_impl->actor, privvid.get ());
          m_impl->privvid = privvid;
      } else {
          visual_log (VISUAL_LOG_DEBUG, "Pointer actvideo->pixels %p", m_impl->actvideo->get_pixels ());
          if (visual_actor_get_video (m_impl->actor)->get_depth () != VISUAL_VIDEO_DEPTH_GL &&
              m_impl->actvideo->get_pixels ()) {
              visual_mem_set (m_impl->actvideo->get_pixels (), 0, m_impl->actvideo->get_size ());
          }
      }

      visual_log (VISUAL_LOG_DEBUG, "Leaving, actor->video->depth: %d actmorph->video->depth: %d",
                  visual_actor_get_video (m_impl->actor)->get_depth (), visual_actor_get_video (m_impl->actmorph)->get_depth ());

      m_impl->morphing = true;
  }

  void Bin::switch_finalize ()
  {
      visual_log (VISUAL_LOG_DEBUG, "Completing actor switch...");

      //visual_object_unref (VISUAL_OBJECT (m_impl->actor));

      /* Copy over the depth to be sure, and for GL plugins */
      /* m_impl->actvideo->set_depth (m_impl->actmorphvideo->get_depth ()); */

      m_impl->actmorphvideo = nullptr;

      if (m_impl->privvid) {
          m_impl->privvid = nullptr;
      }

      m_impl->actor = m_impl->actmorph;
      m_impl->actmorph = nullptr;

      visual_actor_set_video (m_impl->actor, m_impl->actvideo.get ());

      m_impl->morphing = false;

      if (m_impl->morph) {
          visual_object_unref (VISUAL_OBJECT (m_impl->morph));
          m_impl->morph = nullptr;
      }

      visual_log (VISUAL_LOG_DEBUG, " - in finalize - fscking depth from actvideo: %d %d",
                  m_impl->actvideo->get_depth (),
                  m_impl->actvideo->get_bpp ());

      VisVideoDepth depthflag = visual_actor_get_supported_depths (m_impl->actor);
      m_impl->actvideo->set_depth (m_impl->get_suitable_depth (depthflag));
      set_depth (m_impl->actvideo->get_depth ());

      m_impl->depthforcedmain = m_impl->actvideo->get_depth ();
      visual_log (VISUAL_LOG_DEBUG, "m_impl->depthforcedmain in finalize %d", m_impl->depthforcedmain);

      /* FIXME replace with a depth fixer */
      if (m_impl->depthchanged) {
          visual_log (VISUAL_LOG_INFO, "negotiate without event");
          visual_actor_video_negotiate (m_impl->actor, m_impl->depthforcedmain, true, true);
          visual_log (VISUAL_LOG_INFO, "end negotiate without event");
          //sync(false);
      }

      visual_log (VISUAL_LOG_DEBUG, "Leaving...");
  }

  void Bin::use_morph (bool use)
  {
      m_impl->use_morph = use;
  }

  void Bin::switch_set_time (Time const& time)
  {
      m_impl->morphtime = time;
  }

  void Bin::run ()
  {
      visual_return_if_fail (m_impl->actor != nullptr);
      visual_return_if_fail (m_impl->input != nullptr);

      visual_input_run (m_impl->input);

      /* If we have a direct switch, do this BEFORE we run the actor,
       * else we can get into trouble especially with GL, also when
       * switching away from a GL plugin this is needed */
      if (m_impl->morphing) {
          visual_return_if_fail (m_impl->actmorph != nullptr);
          visual_return_if_fail (visual_actor_get_plugin (m_impl->actmorph) != nullptr);

          if (!visual_actor_get_plugin (m_impl->actmorph)->realized) {
              visual_actor_realize (m_impl->actmorph);

              visual_actor_video_negotiate (m_impl->actmorph, m_impl->depthforced, false, true);
          }

          /* When we've got multiple switch events without a sync we need
           * to realize the main actor as well */
          visual_return_if_fail (visual_actor_get_plugin (m_impl->actor) != nullptr);
          if (!visual_actor_get_plugin (m_impl->actor)->realized) {
              visual_actor_realize (m_impl->actor);

              visual_actor_video_negotiate (m_impl->actor, m_impl->depthforced, false, true);
          }

          /* When the style is DIRECT or the context is GL we shouldn't try
           * to morph and instead finalize at once */
          visual_return_if_fail (visual_actor_get_video (m_impl->actor) != nullptr);
          if (!m_impl->use_morph || visual_actor_get_video (m_impl->actor)->get_depth () == VISUAL_VIDEO_DEPTH_GL) {

              switch_finalize ();

              /* We can't start drawing yet, the client needs to catch up with
               * the depth change */
              return;
          }
      }

      visual_actor_realize (m_impl->actor);

      visual_actor_run (m_impl->actor, visual_input_get_audio (m_impl->input));

      if (m_impl->morphing) {
          visual_return_if_fail (m_impl->actmorph != nullptr);
          visual_return_if_fail (visual_actor_get_video (m_impl->actmorph) != nullptr);
          visual_return_if_fail (visual_actor_get_video (m_impl->actor) != nullptr);

          if (m_impl->use_morph &&
              visual_actor_get_video (m_impl->actmorph)->get_depth () != VISUAL_VIDEO_DEPTH_GL &&
              visual_actor_get_video (m_impl->actor)->get_depth () != VISUAL_VIDEO_DEPTH_GL) {

              visual_actor_run (m_impl->actmorph, visual_input_get_audio (m_impl->input));

              if (!m_impl->morph || !m_impl->morph->plugin) {
                  switch_finalize ();
                  return;
              }

              /* Same goes for the morph, we realize it here for depth changes
               * (especially the openGL case */
              visual_morph_realize (m_impl->morph);
              visual_morph_run (m_impl->morph, visual_input_get_audio (m_impl->input), visual_actor_get_video (m_impl->actor), visual_actor_get_video (m_impl->actmorph));

              if (visual_morph_is_done (m_impl->morph))
                  switch_finalize ();
          } else {
              /* visual_bin_switch_finalize (bin); */
          }
      }
  }

} // LV namespace
