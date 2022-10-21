#!/bin/bash
#
# Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#

set -e

IMAGE=build-b2-clang12

CONTAINER=builder-$IMAGE

docker start mysql
docker start $CONTAINER || docker run -dit --name $CONTAINER -v ~/workspace/mysql:/opt/boost-mysql -v /var/run/mysqld:/var/run/mysqld anarthal/$IMAGE
docker network connect my-net $CONTAINER || echo "Network already connected"
docker exec $CONTAINER /opt/boost-mysql/tools/scripts/build_b2_local_docker.sh
