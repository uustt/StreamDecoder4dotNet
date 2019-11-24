#include "Demux.h"
#include <iostream>
#include "StreamDecoder.h"
#include "Tools.h"
#include "Packet.h"
extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/error.h>
#include <libavutil/time.h>
}
#define BUFF_SIZE 65536
#define  URL_LENGTH 1280
using namespace  std;
Demux::Demux(int cacheSize, int demuxTimeout, bool alwaysWaitBitStream, int waitBitStreamTimeout)
{
	this->dataCacheSize = cacheSize;
	this->demuxTimeout = demuxTimeout;
	this->alwaysWaitBitStream = alwaysWaitBitStream;
	this->alwaysWaitBitStream = waitBitStreamTimeout;
	//��ʼ�� ��Ƶ����ַ ����
	url = new char[URL_LENGTH];
	memset(url, 0, URL_LENGTH);

	dataCache = new SCharList(this->dataCacheSize);
}


Demux::~Demux()
{
	Close();
	cout << "~Demux" << endl;
}

void Demux::Clear()
{
	quitSignal = true;
	while (isInOpenFunc)
	{
		Tools::Get()->Sleep(1);
	}
	mux.lock();
	
	if (afc)
	{
		//�ͷŲ����� �ڲ������avformat_free_context
		avformat_close_input(&afc);
		//avformat_free_context(afc);
		afc = NULL;
	}

	if (avio)
	{
		av_free(avio->buffer);
		//�ͷŲ�����
		avio_context_free(&avio);
		avio = NULL;
	}

	if(dataCache)
		dataCache->Clear();

	mux.unlock();
}

//��������
void Demux::Close()
{
	
	Clear();
	mux.lock();
	delete url;
	url = NULL;

	delete dataCache;
	dataCache = NULL;

	mux.unlock();
}

//���̰߳�ȫ
bool Demux::Open(char* url)
{
	isInOpenFunc = true;
	if (demuxed)
	{
		cout << "�Ѿ����װ�ɹ�" << endl;
		isInOpenFunc = false;
		return false;
	}

	if (isDemuxing)
	{
		isInOpenFunc = false;
		StreamDecoder::Get()->PushLog2Net(Warning, "Demuxing please wait!");
		return false;
	}
	//���뵽���װ
	isDemuxing = true;
	quitSignal = false;
	dataCache->Clear();

	mux.lock();
	if (afc)
	{
		mux.unlock();
		cout << "���ش��� afc ����" << endl;
		isDemuxing = false;
		isInOpenFunc = false;
		return false;
	}
	afc = avformat_alloc_context();
	if (!afc)
	{
		mux.unlock();
		StreamDecoder::Get()->PushLog2Net(Warning, "avformat_alloc_context failed!");
		isDemuxing = false;
		isInOpenFunc = false;
		return false;
	}
	startTime = av_gettime();
	bool isSuccess = true;

	//bit ��
	if (url == NULL)
	{
		memset(this->url, 0, URL_LENGTH);
	
		isSuccess = ProbeInputBuffer();

	}
	//������
	else
	{
		memset(this->url, 0, URL_LENGTH);
		memcpy(this->url, url, strlen(url));
		afc->interrupt_callback.opaque = this;
		afc->interrupt_callback.callback = interrupt_cb;
		StreamDecoder::Get()->PushLog2Net(Info, this->url);
		
		isSuccess = BeginDemux();
	}

	mux.unlock();

	isDemuxing = false;
	isInOpenFunc = false;
	if (isSuccess)
	{
		if (quitSignal)
		{
			cout << "�ֶ�ֹͣ" << endl;
			isSuccess = false;
			Clear();
		}
		else
		{
			DemuxSuccess();
		}
	}
	else Clear();

	return isSuccess;
}



bool Demux::PushStream2Cache(char* data, int len)
{
	dataCacheMux.lock();
	if (!dataCache || dataCache->size() + len > dataCacheSize)
	{
		dataCacheMux.unlock();
		return false;
	}
	dataCache->push_back(data, len);
	dataCacheMux.unlock();
	return true;
}

int Demux::interrupt_cb(void* opaque)
{
	Demux* demux = (Demux*)opaque;
	if (!demux) return 1;
	if (demux->quitSignal) return 1;
	return 0;
}

int Demux::read_packet(void *opaque, uint8_t *buf, int buf_size)
{
	Demux* demux = (Demux*)opaque;
	if (!demux) return 0;
	if (demux->quitSignal) return 0;
	size_t lastT = av_gettime();
	//��û�����ݵȴ�
	while (demux->dataCache->size() <= 0)
	{
		if (demux->quitSignal) return 0;
		//���ڽ��װ
		if (demux->isDemuxing)
		{
			if (av_gettime() - demux->startTime > demux->demuxTimeout * 1000)
			{
				//cout << "return 0" << endl;
				return 0;
			}
			else
			{
				//cout << "continue" << endl;
				continue;
			}
		}
		//����av_read_frame
		else
		{
			if (demux->alwaysWaitBitStream)
			{
				continue;
			}
			else
			{
				//��ʱ������������Ϊ���ж�
				if (av_gettime() - lastT > demux->waitBitStreamTimeout * 1000)
				{
					return 0;
				}
				else
				{
					continue;
				}
			}
		}
		//�ȴ�
		Tools::Get()->Sleep(1);
	}

	demux->dataCacheMux.lock();
	int size = buf_size < demux->dataCache->size() ? buf_size : demux->dataCache->size();
	memcpy(buf, demux->dataCache->arr, size);
	demux->dataCache->pop_front(size);
	demux->dataCacheMux.unlock();

	return size;
}
bool Demux::ProbeInputBuffer()
{


	//̽������ʽ  
	//TODOҪ��Ҫ�ͷţ�
	AVInputFormat *piFmt = NULL;
	//AVInputFormat* in_fmt = av_find_input_format("h265");

	unsigned char* readBuff = (unsigned char*)av_malloc(BUFF_SIZE);

	if (avio)
	{
		cout << "���ش��� avio ����" << endl;

		return false;
	}
	//���� AVIOContext�� ʹ��avio_context_free()�ͷŲ�����, readBuff�ͷ���av_free(avio->buffer) ��Ҫ����ȫ�ֵı����洢���ͷ�ȫ�ֵĻ����
	avio = avio_alloc_context(readBuff, BUFF_SIZE, 0, this, read_packet, NULL, NULL);
	if (!avio)
	{
		StreamDecoder::Get()->PushLog2Net(Warning, "avio_alloc_context failed!");

		return false;
	}
	afc->pb = avio;

	//The caller has supplied a custom AVIOContext, don't avio_close() it.
	//AVFMT_FLAG_CUSTOM_IO
	afc->flags = AVFMT_FLAG_CUSTOM_IO;

	int ret = av_probe_input_buffer(avio, &piFmt, NULL, NULL, 0, 0);

	if (ret < 0)
	{
		StreamDecoder::Get()->PushLog2Net(Warning, Tools::Get()->av_strerror2(ret));

		return false;
	}
	//��ӡ����Ϣ
	char info[100];
	sprintf(info, "Probe stream info success! format:%s[%s]", piFmt->name, piFmt->long_name);
	StreamDecoder::Get()->PushLog2Net(Info, info);
	return BeginDemux();
}

bool Demux::BeginDemux()
{
	//AVDictionary *opts = NULL;
	//����rtsp����tcpЭ���
	//av_dict_set(&opts, "rtsp_transport", "tcp", 0);
	//������ʱʱ��
	//av_dict_set(&opts, "max_delay", "500", 0);
	//av_dict_set_int(&opts, "stimeout", 5000, 0);

	int ret = avformat_open_input(&afc, url, NULL, NULL);

	//av_dict_free(&opts);
	if (ret < 0)
	{
		StreamDecoder::Get()->PushLog2Net(Warning, "avformat_open_input failed!");
		return false;
	}
	//cout << "avformat_open_input open Success" << endl;
	StreamDecoder::Get()->PushLog2Net(Warning, "avformat_open_input open Success");
	//��ȡһ����Ƶ ��ȡ����Ϣ
	ret = avformat_find_stream_info(afc, NULL);
	StreamDecoder::Get()->PushLog2Net(Warning, "avformat_find_stream_info Success");

	if (ret < 0)
	{
		StreamDecoder::Get()->PushLog2Net(Warning, Tools::Get()->av_strerror2(ret));
		return false;
	}

	//��ӡ����ϸ��Ϣ
	av_dump_format(afc, 0, NULL, 0);


	//videoStreamIndex = av_find_best_stream(afc, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);*/


	//AVCodecParameters *para = avcodec_parameters_alloc();
	//avcodec_parameters_copy(para, afc->streams[videoStreamIndex]->codecpar);

	return true;
}

void Demux::DemuxSuccess()
{
	//���װ�ɹ�
	demuxed = true;
}