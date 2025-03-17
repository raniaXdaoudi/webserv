# **************************************************************************** #
#                                                                              #
#                                                         :::      ::::::::    #
#    Makefile                                           :+:      :+:    :+:    #
#                                                     +:+ +:+         +:+      #
#    By: rania <rania@student.42.fr>                +#+  +:+       +#+         #
#                                                 +#+#+#+#+#+   +#+            #
#    Created: 2025/01/31 12:13:00 by rania             #+#    #+#              #
#    Updated: 2025/03/03 16:49:05 by rania            ###   ########.fr        #
#                                                                              #
# **************************************************************************** #

# Nom de l'exécutable
NAME = webserv

# Compilateur et flags
CXX = c++
CXXFLAGS = -Wall -Wextra -Werror -std=c++98 -g3 -I$(INCDIR)

# Dossiers
SRCDIR = src
OBJDIR = obj
INCDIR = include

# Fichiers sources de départ (on ajoutera les autres plus tard)
SRCS = $(SRCDIR)/main.cpp \
		$(SRCDIR)/Server.cpp \
		$(SRCDIR)/Request.cpp \
		$(SRCDIR)/Response.cpp \
		$(SRCDIR)/ConfigParser.cpp \
		$(SRCDIR)/Utils.cpp \
		$(SRCDIR)/Connection.cpp \
		$(SRCDIR)/Route.cpp \
		$(SRCDIR)/Logger.cpp

# Fichiers objets (on changera .cpp en .o)
OBJS = $(SRCS:$(SRCDIR)/%.cpp=$(OBJDIR)/%.o)

# Règle principale
all: $(NAME)

# Compilation des objets
$(OBJDIR)/%.o: $(SRCDIR)/%.cpp | $(OBJDIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Création du dossier obj si nécessaire
$(OBJDIR):
	mkdir -p $(OBJDIR)

# Création de l'exécutable
$(NAME): $(OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) -o $(NAME)

# Nettoyage des fichiers objets
clean:
	rm -rf $(OBJDIR)

# Nettoyage complet
fclean: clean
	rm -f $(NAME)

# Recompilation complète
re: fclean all

.PHONY: all clean fclean re
