FROM gentoo/stage3:latest

LABEL platform agnostic container

RUN emerge --sync

RUN echo 'CFLAGS="-march=native -O3 -pipe"' > /etc/make.conf
RUN echo 'CXXFLAGS="${CFLAGS}"' >> /etc/make.conf
RUN echo 'USE="default-libcxx"' >> /etc/make.conf


RUN echo "dev-libs/boost **" > /etc/portage/package.accept_keywords/boost
RUN echo "dev-util/boost-build **" >> /etc/portage/package.accept_keywords/boost
RUN echo "dev-libs/boost test" > /etc/portage/package.use/boost
RUN echo "sys-devel/gcc **" >> /etc/portage/package.accept_keywords/gcc
RUN echo "sys-devel/clang* **" >> /etc/portage/package.accept_keywords/clang
RUN echo "sys-devel/llvm* **" >> /etc/portage/package.accept_keywords/clang
RUN echo "sys-libs/libcxx* **" >> /etc/portage/package.accept_keywords/clang
RUN echo "sys-libs/compiler-rt* **" >> /etc/portage/package.accept_keywords/clang
RUN echo "sys-libs/libomp **" >> /etc/portage/package.accept_keywords/clang

RUN emerge clang -j`nproc`
#RUN emerge sys-devel/gcc:13 -u

RUN emerge cmake
RUN emerge dev-vcs/git

#RUN emerge =dev-libs/boost-1.79.0
RUN emerge dev-lang/ruby
RUN emerge dev-libs/icu


#RUN emerge leveldb

#RUN emerge llvm
RUN emerge bash-completion
#RUN emerge clang

RUN emerge eix
RUN eix-update

#RUN emerge --unmerge sys-devel/gcc
