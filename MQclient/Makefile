.PHONY: PublishClient ConsumeClient
all:PublishClient ConsumeClient
PublishClient : PublishClient.cpp ../MQcommon/mq_proto.pb.cc ../MQcommon/mq_msg.pb.cc ../MQthird/include/muduo/proto/codec.cc
	g++ -g -std=c++11 $^ -o $@ -I../MQthird/include -L../MQthird/lib  -lmuduo_net -lmuduo_base -pthread -lprotobuf -lz
ConsumeClient: ConsumeClient.cpp ../MQcommon/mq_proto.pb.cc ../MQcommon/mq_msg.pb.cc ../MQthird/include/muduo/proto/codec.cc
	g++ -g -std=c++11 $^ -o $@ -I../MQthird/include -L../MQthird/lib  -lmuduo_net -lmuduo_base -pthread -lprotobuf -lz
.PHONY:clean
clean:
	rm	-f	ConsumeClient
	rm	-f	PublishClient