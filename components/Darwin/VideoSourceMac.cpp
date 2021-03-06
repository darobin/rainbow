/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Rainbow.
 *
 * The Initial Developer of the Original Code is Mozilla Labs.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Anant Narayanan <anant@kix.in>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "VideoSourceMac.h"

VideoSourceMac::VideoSourceMac(int w, int h)
    : VideoSource(w, h)
{
    /* Setup video devices */
    int nd = 0;
    struct vidcap_sapi_info sapi_info;

    g2g = PR_FALSE;
    if (!(state = vidcap_initialize())) {
        PR_LOG(log, PR_LOG_NOTICE, ("Could not initialize vidcap\n"));
        return;
    }
    if (!(sapi = vidcap_sapi_acquire(state, 0))) {
        PR_LOG(log, PR_LOG_NOTICE, ("Failed to acquire default sapi\n"));
        return;
    }
    if (vidcap_sapi_info_get(sapi, &sapi_info)) {
        PR_LOG(log, PR_LOG_NOTICE, ("Failed to get default sapi info\n"));
        return;
    }

    nd = vidcap_src_list_update(sapi);
    if (nd < 0) {
        PR_LOG(log, PR_LOG_NOTICE, ("Failed vidcap_src_list_update\n"));
        return;
    } else if (nd == 0) {
        /* No video capture device available */
        PR_LOG(log, PR_LOG_DEBUG, ("No video devices were found!\n"));
        return;
    } else {
        if (!(sources = (struct vidcap_src_info *)
            PR_Calloc(nd, sizeof(struct vidcap_src_info)))) {
            return;
        }
        if (vidcap_src_list_get(sapi, nd, sources)) {
            PR_Free(sources);
            PR_LOG(log, PR_LOG_NOTICE, ("Failed vidcap_src_list_get\n"));
            return;
        }
    }

    /* Get default fmt_info, set our height/width and set it back */
    if (!(source = vidcap_src_acquire(sapi, &sources[0]))) {
        PR_LOG(log, PR_LOG_NOTICE, ("Failed vidcap_src_acquire\n"));
        PR_Free(sources);
        return;
    }
    if (vidcap_format_bind(source, 0)) {
        PR_LOG(log, PR_LOG_NOTICE, ("Failed vidcap_format_bind\n"));
        PR_Free(sources);
        return;
    }
    if (vidcap_format_info_get(source, &fmt_info)) {
        PR_LOG(log, PR_LOG_NOTICE, ("Failed vidcap_format_bind\n"));
        PR_Free(sources);
        return;
    }

    fmt_info.width = width;
    fmt_info.height = height;
    fmt_info.fourcc = VIDCAP_FOURCC_I420;
    fps_n = fmt_info.fps_numerator;
    fps_d = fmt_info.fps_denominator;
    
    if (vidcap_format_bind(source, &fmt_info)) {
        PR_LOG(log, PR_LOG_NOTICE, ("Failed vidcap_format_bind\n"));
        PR_Free(sources);
        return;
    }

    vidcap_src_release(source);
    g2g = PR_TRUE;
}

VideoSourceMac::~VideoSourceMac()
{
    vidcap_sapi_release(sapi);
    vidcap_destroy(state);
    PR_Free(sources);
}

nsresult
VideoSourceMac::Start(
    nsIOutputStream *pipe, nsIDOMCanvasRenderingContext2D *ctx)
{
    if (!g2g)
        return NS_ERROR_FAILURE;

    vCanvas = ctx;
    if (!(source = vidcap_src_acquire(sapi, &sources[0]))) {
        PR_LOG(log, PR_LOG_NOTICE, ("Failed vidcap_src_acquire\n"));
        return NS_ERROR_FAILURE;
    }

    if (vidcap_format_bind(source, &fmt_info)) {
        PR_LOG(log, PR_LOG_NOTICE, ("Failed vidcap_format_bind\n"));
        return NS_ERROR_FAILURE;
    }

    if (vidcap_src_capture_start(source, VideoSourceMac::Callback, this)) {
        PR_LOG(log, PR_LOG_NOTICE, ("Failed vidcap_src_capture_start\n"));
        return NS_ERROR_FAILURE;
    }

    output = pipe;
    return NS_OK;
}

nsresult
VideoSourceMac::Stop()
{
    if (!g2g)
        return NS_ERROR_FAILURE;

    if (vidcap_src_capture_stop(source)) {
        PR_LOG(log, PR_LOG_NOTICE, ("Failed vidcap_src_capture_stop\n"));
        return NS_ERROR_FAILURE;
    }
    vidcap_src_release(source);

    return NS_OK;
}

int
VideoSourceMac::Callback(
    vidcap_src *src, void *data, struct vidcap_capture_info *video)
{
    nsresult rv;
    PRUint32 wr;
    VideoSourceMac *vsm = static_cast<VideoSourceMac*>(data);

    /* Write to pipe */
    rv = vsm->output->Write(
        (const char *)video->video_data, video->video_data_size, &wr
    );

    /* Write to canvas, if needed */
    int fsize = vsm->width * vsm->height * 4;
    if (vsm->vCanvas) {
        /* Convert RGB32 to i420 */
        nsAutoArrayPtr<PRUint8> rgb32(new PRUint8[fsize]);
        I420toRGB32(vsm->width, vsm->height,
            (const char *)video->video_data, (char *)rgb32.get());

        nsCOMPtr<nsIRunnable> render = new CanvasRenderer(
            vsm->vCanvas, vsm->width, vsm->height, rgb32, fsize
        );
        rv = NS_DispatchToMainThread(render);
    }

    return 0;
}

