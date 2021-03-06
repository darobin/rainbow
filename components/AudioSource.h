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

/*
 * Interface for video sources to implement. Not really neccessary because we
 * use only portaudio for all platforms, but just in case we switch to something
 * else in the future.
 * TODO: Figure out if and how to do device selection
 */
#include <prlog.h>
#include <nsError.h>
#include <nsIOutputStream.h>

/* Defaults */
#define NUM_CHANNELS    1
#define FRAMES_BUFFER   1024

#define SAMPLE          PRInt16
#define SAMPLE_RATE     22050
#define SAMPLE_QUALITY  (float)(0.4)

class AudioSource {
public:
    /* Reuse constructor and frame size getter */
    AudioSource(int channels, int rate);
    int GetFrameSize();
    PRUint32 GetRate();
    PRUint32 GetChannels();

    /* Implement these two. Write 2byte, n-channel audio to pipe */
    virtual nsresult Stop() = 0;
    virtual nsresult Start(nsIOutputStream *pipe) = 0;

protected:
    /* You MUST set these two values in the constructor! */
    int rate;
    int channels;

    PRLogModuleInfo *log;

};

