# 编译器
CXX = g++
# 编译选项
CXXFLAGS = -Wall -Wextra -std=c++11
# 静态库路径
LIB_PATH = ./
# 静态库名称（去掉 'lib' 前缀和文件扩展名）
BOOST_LIB = boost_system

# 目标文件
TARGETS2 = perf
# TARGETS1 = loop

# 源文件列表
# SRCS_loop = server_loop.cpp domain_server.cpp
SRCS_perf = domain_client.cpp perf_test.cpp

# 生成目标文件列表
# OBJS_loop = $(SRCS_loop:.cpp=.o)
OBJS_perf = $(SRCS_perf:.cpp=.o)

# 链接规则
# loop: $(OBJS_loop)
# 	$(CXX) $(OBJS_loop) -o $@ -L$(LIB_PATH) -l$(BOOST_LIB) -static

perf: $(OBJS_perf)
	$(CXX) $(OBJS_perf) -o $@ -L$(LIB_PATH) -lpthread -g -l$(BOOST_LIB) -static

# 编译规则
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# 清理规则
clean:
	rm -f $(OBJS_loop) $(OBJS_perf) $(TARGETS1) $(TARGETS2)
