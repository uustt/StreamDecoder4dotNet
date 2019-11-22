﻿using System.Collections;
using System.Collections.Generic;
using UnityEngine;
using SStreamDecoder;
using UnityEngine.UI;
using System.Threading;
using System.IO;

public class GameManager : MonoBehaviour
{

    private StreamPlayer player;
    public int bitStreamCacheSize = 1000000;
    public int readBuffSize = 1024;
    public Text tipText;
    public string localPath = "F:/HTTPServer/Faded.mp4";
    public string netUrl = "rtmp://192.168.30.135/live/test";
    private bool isExit = false;
    
    public RawImage rimg;
    private Material mat;
    private int width = 0;
    private int height = 0;
    private Texture2D ytex, utex, vtex;
    // Use this for initialization
    void Start()
    {

#if UNITY_EDITOR
        StreamDecoder.dllPath = Application.streamingAssetsPath + "/../../../../bin/";
#else
        StreamDecoder.dllPath = Application.streamingAssetsPath + "/../../../../../bin/";
#endif

        //加载动态库
        if (!StreamDecoder.LoadLibrary())
        {
            tipText.text = "FFmpeg动态链接库加载失败";
            return;
        }
        //StreamDecoder.InitStreamDecoder();
        //StreamDecoder.logEvent += StreamDecoderLog;
        //StreamDecoder.drawEvent += OnDrawFrame;

        mat = rimg.material;

        
    }

    // Update is called once per frame
    void Update()
    {
        if (Input.GetKeyDown(KeyCode.Space))
        {
           
        }
    }
    private void OnDestroy()
    {
        isExit = true;
        DeleteSession();
        //释放动态库
        StreamDecoder.FreeLibrary();
    }

    [Space]
    [Space]
    [SerializeField]
    private int demuxTimeout = 2000;
    [SerializeField]
    private int pushFrameInterval = 20;
    [SerializeField]
    private int waitBitStreamTimeout = 1000;
    [SerializeField]
    private bool alwaysWaitBitStream = true;
    public void CreateSession()
    {
        if (player != null) return;
        //唯一ID
        player = StreamPlayer.CreateSession(1, bitStreamCacheSize, OnDrawFrame);
        player.SetOption(OptionType.DemuxTimeout, demuxTimeout);
        player.SetOption(OptionType.PushFrameInterval, pushFrameInterval);
        player.SetOption(OptionType.WaitBitStreamTimeout, waitBitStreamTimeout);
        player.SetOption(OptionType.AlwaysWaitBitStream, alwaysWaitBitStream ? 1 : 0);
    }
    public void DeleteSession()
    {
        if (player == null) return;

        StreamPlayer.DeleteSession(ref player);
    }

    public void TryBitStreamDemux()
    {
        if (player == null) return;
        Debug.Log(player.TryBitStreamDemux());
    }
    public void TryNetStreamDemux()
    {
        if (player == null) return;
        Debug.Log(player.TryNetStreamDemux(netUrl));
    }

  
    public void BeginDecode()
    {
        if (player == null) return;
        player.BeginDecode();
    }
    public void StopDecode()
    {
        if (player == null) return;
        player.StopDecode();
    }
    public void GetCacheFreeSize()
    {
        if (player == null) return;
        Debug.Log(player.GetCacheFreeSize());
    }

    #region Send Data
    private bool isSending = false;
    public void StartSendData()
    {
        if (isSending) return;
        isSending = true;
        new Thread(run).Start();
    }
    public void EndSendData()
    {
        if (!isSending) return;
        isSending = false;
    }

    private void run()
    {
        Debug.Log("Begin send Data");

        if (!File.Exists(localPath))
        {
            Debug.Log(localPath + " not exists");
            return;
        }
        FileStream file = new FileStream(localPath, FileMode.Open);
        byte[] readBuff = new byte[readBuffSize];
        int count = 0;
        while (!isExit && isSending)
        {
            int ret = 0;
            try
            {
                ret = file.Read(readBuff, 0, readBuffSize);
            }
            catch (System.Exception ex)
            {
                Debug.LogWarning(ex);
                return;
            }
            if (ret <= 0)
            {
                break;
            }
            count += ret;
            //处理数据
            while (!isExit && isSending)
            {
                if(player == null)
                {
                    break;
                }
                if (player.PushStream2Cache(readBuff, ret)) break;
              
                Thread.Sleep(1);
                continue;
            }
            Thread.Sleep(1);
        }
        file.Dispose();
        file.Close();
        Debug.Log("Stop send data");
    }
    #endregion

    //private void StreamDecoderLog(int level, string log)
    //{
    //    if (level == 0)
    //    {
    //        tipText.text = string.Format("<color=#ffffff>{0}</color>", log);
    //        Debug.Log(log);
    //    }
    //    else if (level == 1)
    //    {
    //        tipText.text = string.Format("<color=#ffff00>{0}</color>", log);
    //        Debug.LogWarning(log);
    //    }
    //    else
    //    {
    //        tipText.text = string.Format("<color=#ff0000>{0}</color>", log);
    //        Debug.LogError(log);
    //    }
    //}
    public void OnDrawFrame(DotNetFrame frame)
    {

        if (width != frame.width || height != frame.height)
        {
            width = frame.width;
            height = frame.height;
            ytex = new Texture2D(width, height, TextureFormat.R8, false);
            utex = new Texture2D(width / 2, height / 2, TextureFormat.R8, false);
            vtex = new Texture2D(width / 2, height / 2, TextureFormat.R8, false);
        }
        ytex.LoadRawTextureData(frame.frame_y, width * height);
        ytex.Apply();
        utex.LoadRawTextureData(frame.frame_u, width * height / 4);
        utex.Apply();
        vtex.LoadRawTextureData(frame.frame_v, width * height / 4);
        vtex.Apply();
        mat.SetTexture("_YTex", ytex);
        mat.SetTexture("_UTex", utex);
        mat.SetTexture("_VTex", vtex);
    }
}
